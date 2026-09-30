#include <backends/opengl/texture.h>
#include <backends/opengl/vertex-array-object.h>

#include <internals/exceptions.h>
#include <limits>
#include <utility>

template <typename T, uint64_t offset>
constexpr uint64_t get_length() {
    return sizeof(T) * offset;
}

template <typename T, uint64_t offset>
constexpr void* glBufferOffset() {
    return (void*)get_length<T, offset>();
}
namespace CE {
    using namespace VAONumbers;

    namespace {
        RenderAPIs::OpenGLHandle create_vertex_array(const std::shared_ptr<RenderAPIs::OpenGLResourceLifetime>& lifetime) {
            if (!lifetime)
                throw Exceptions::invalid_args(CE_HERE, "OpenGL upload needs a resource lifetime");
            lifetime->require_current();
            GLuint id = 0;
            glGenVertexArrays(1, &id);
            try {
                return {lifetime, RenderAPIs::GLResourceKind::VertexArray, id};
            }
            catch (...) {
                if (id)
                    glDeleteVertexArrays(1, &id);
                throw;
            }
        }

        RenderAPIs::OpenGLHandle create_buffer(const std::shared_ptr<RenderAPIs::OpenGLResourceLifetime>& lifetime) {
            if (!lifetime)
                throw Exceptions::invalid_args(CE_HERE, "OpenGL upload needs a resource lifetime");
            lifetime->require_current();
            GLuint id = 0;
            glGenBuffers(1, &id);
            try {
                return {lifetime, RenderAPIs::GLResourceKind::Buffer, id};
            }
            catch (...) {
                if (id)
                    glDeleteBuffers(1, &id);
                throw;
            }
        }
    }

    void VAO::bind(const Assets::Image& image) const {
        const auto* texture = dynamic_cast<const Assets::Texture*>(&image);
        if (!texture)
            throw Exceptions::invalid_args(CE_HERE, "An OpenGL vertex array requires an OpenGL texture");
        glBindVertexArray(vao_.id());
        texture->bind();
    }

    void VAO::draw(const std::size_t first_vertex, const std::size_t vertex_count) const {
        (void)vao_.id();
        if (first_vertex > vertex_count_ || vertex_count > vertex_count_ - first_vertex)
            throw Exceptions::invalid_args(CE_HERE, "Draw range exceeds uploaded geometry");
        if (type != flat)
            throw Exceptions::invalid_args(CE_HERE, "Indexed meshes cannot be drawn as 2D geometry");
        const GLenum mode = topology_ == Assets::PrimitiveTopology::TriangleStrip ? GL_TRIANGLE_STRIP : GL_TRIANGLES;
        glDrawArrays(mode, static_cast<GLint>(first_vertex), static_cast<GLsizei>(vertex_count));
    }

    namespace {
        std::span<const Vertex2D> vertex_view(const std::shared_ptr<Vertex2D>& vertices, const std::uint32_t count) {
            if (!vertices || count == 0)
                throw Exceptions::invalid_args(CE_HERE, "Cannot upload empty geometry");
            return {vertices.get(), count};
        }
    }

    VAO::VAO(std::shared_ptr<RenderAPIs::OpenGLResourceLifetime> lifetime,
             std::shared_ptr<Vertex2D> vertices,
             const uint32_t num_vertices,
             const Assets::PrimitiveTopology topology)
        : VAO(std::move(lifetime), vertex_view(vertices, num_vertices), topology) {}

    VAO::VAO(std::shared_ptr<RenderAPIs::OpenGLResourceLifetime> lifetime,
             const std::span<const Vertex2D> vertices,
             const Assets::PrimitiveTopology topology)
        : type(flat), topology_(topology), vertex_count_(vertices.size()) {
        if (vertices.empty() || !vertices.data() || vertices.size() > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()) ||
            vertices.size() > static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max()) / sizeof(Vertex2D))
            throw Exceptions::invalid_args(CE_HERE, "2D geometry exceeds supported buffer/draw sizes");
        if (topology != Assets::PrimitiveTopology::Triangles && topology != Assets::PrimitiveTopology::TriangleStrip)
            throw Exceptions::invalid_args(CE_HERE, "Unsupported 2D primitive topology");
        // Copy the view before return; the GPU resource retains no CPU owner.
        constexpr GLsizei byte_stride = sizeof(Vertex2D);
        const auto vertices_bytes = static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex2D));
        vao_ = create_vertex_array(lifetime);
        glBindVertexArray(vao_.id());
        vbo_[1] = create_buffer(lifetime);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_[1].id());
        glBufferData(GL_ARRAY_BUFFER, vertices_bytes, vertices.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, byte_stride, glBufferOffset<float, 0>());
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, byte_stride, glBufferOffset<float, 3>());
    }

    VAO::VAO(std::shared_ptr<RenderAPIs::OpenGLResourceLifetime> lifetime,
             std::shared_ptr<Vertex3D> vertices,
             uint32_t num_vertices,
             std::shared_ptr<uint32_t> indices,
             uint32_t num_indices)
        : type(mesh), vertex_count_(num_vertices) {
        // Copy mesh indices and interleaved 3D vertices; the VAO retains the
        // element buffer binding along with position, normal, and UV layout.
        if (!vertices || !indices || num_vertices == 0 || num_indices == 0 ||
            num_vertices > static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max()) / sizeof(Vertex3D) ||
            num_indices > static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max()) / sizeof(uint32_t))
            throw Exceptions::invalid_args(CE_HERE, "Mesh buffers need nonempty addressable data");
        constexpr GLsizei byte_stride = sizeof(Vertex3D);
        const auto vertices_bytes = static_cast<GLsizeiptr>(num_vertices) * sizeof(Vertex3D);
        const auto size_vbo_indices = static_cast<GLsizeiptr>(num_indices) * sizeof(uint32_t);
        vao_ = create_vertex_array(lifetime);
        glBindVertexArray(vao_.id());
        vbo_[0] = create_buffer(lifetime);
        vbo_[1] = create_buffer(lifetime);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_[0].id());
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, size_vbo_indices, indices.get(), GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_[1].id());
        glBufferData(GL_ARRAY_BUFFER, vertices_bytes, vertices.get(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, byte_stride, glBufferOffset<float, 0>());
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, byte_stride, glBufferOffset<float, 3>());
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, byte_stride, glBufferOffset<float, 6>());
    }
}
