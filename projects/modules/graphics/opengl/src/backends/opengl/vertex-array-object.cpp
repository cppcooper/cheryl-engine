#include <backends/opengl/vertex-array-object.h>
#include "upload-check.h"

#include <internals/exceptions.h>
#include <limits>
#include <utility>

template <typename T, uint64_t offset> constexpr uint64_t get_length() {
    return sizeof(T) * offset;
}

template <typename T, uint64_t offset> constexpr void* glBufferOffset() {
    return (void*)get_length<T, offset>();
}

namespace CE {
    using namespace VAONumbers;

    namespace {
        RenderAPIs::OpenGLHandle create_vertex_array(const std::shared_ptr<RenderAPIs::OpenGLResourceLifetime>& lifetime) {
            if (!lifetime)
                throw Exceptions::invalid_args(CE_HERE, "OpenGL upload needs a resource lifetime");
            lifetime->require_current();
            RenderAPIs::require_no_gl_error("Cannot create a vertex array with pending OpenGL errors");
            GLuint id = 0;
            glGenVertexArrays(1, &id);
            try {
                RenderAPIs::require_no_gl_error("OpenGL vertex array creation failed");
                return {lifetime, RenderAPIs::GLResourceKind::VertexArray, id};
            } catch (...) {
                lifetime->discard_untracked(RenderAPIs::GLResourceKind::VertexArray, id);
                throw;
            }
        }

        RenderAPIs::OpenGLHandle create_buffer(const std::shared_ptr<RenderAPIs::OpenGLResourceLifetime>& lifetime) {
            if (!lifetime)
                throw Exceptions::invalid_args(CE_HERE, "OpenGL upload needs a resource lifetime");
            lifetime->require_current();
            RenderAPIs::require_no_gl_error("Cannot create a buffer with pending OpenGL errors");
            GLuint id = 0;
            glGenBuffers(1, &id);
            try {
                RenderAPIs::require_no_gl_error("OpenGL buffer creation failed");
                return {lifetime, RenderAPIs::GLResourceKind::Buffer, id};
            } catch (...) {
                lifetime->discard_untracked(RenderAPIs::GLResourceKind::Buffer, id);
                throw;
            }
        }
    }

    void VAO::bind() const {
        glBindVertexArray(vao_.id());
    }

    void VAO::require_draw(const std::size_t first_vertex, const std::size_t vertex_count) const {
        (void)vao_.id();
        if (first_vertex > vertex_count_ || vertex_count > vertex_count_ - first_vertex)
            throw Exceptions::invalid_args(CE_HERE, "Draw range exceeds uploaded geometry");
        if (type != flat)
            throw Exceptions::invalid_args(CE_HERE, "Indexed meshes cannot be drawn as 2D geometry");
    }

    void VAO::draw(const std::size_t first_vertex, const std::size_t vertex_count) const {
        require_draw(first_vertex, vertex_count);
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

    VAO::VAO(
        std::shared_ptr<RenderAPIs::OpenGLResourceLifetime> lifetime,
        std::shared_ptr<Vertex2D> vertices,
        const uint32_t num_vertices,
        const Assets::PrimitiveTopology topology
    )
    : VAO(std::move(lifetime), vertex_view(vertices, num_vertices), topology) {}

    VAO::VAO(
        std::shared_ptr<RenderAPIs::OpenGLResourceLifetime> lifetime,
        const std::span<const Vertex2D> vertices,
        const Assets::PrimitiveTopology topology
    )
    : type(flat), topology_(topology), vertex_count_(vertices.size()) {
        upload_flat(lifetime, vertices.data(), sizeof(Vertex2D));
    }

    VAO::VAO(
        std::shared_ptr<RenderAPIs::OpenGLResourceLifetime> lifetime,
        const std::span<const Vertex2DColor> vertices,
        const Assets::PrimitiveTopology topology
    )
    : type(flat), topology_(topology), layout_(Assets::VertexLayout2D::Position3UV2Color4), vertex_count_(vertices.size()) {
        upload_flat(lifetime, vertices.data(), sizeof(Vertex2DColor));
    }

    void VAO::upload_flat(
        const std::shared_ptr<RenderAPIs::OpenGLResourceLifetime>& lifetime,
        const void* vertices,
        const std::size_t byte_stride
    ) {
        if (vertex_count_ == 0 || !vertices || vertex_count_ > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()) ||
            vertex_count_ > static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max()) / byte_stride)
            throw Exceptions::invalid_args(CE_HERE, "2D geometry exceeds supported buffer/draw sizes");
        if (topology_ != Assets::PrimitiveTopology::Triangles && topology_ != Assets::PrimitiveTopology::TriangleStrip)
            throw Exceptions::invalid_args(CE_HERE, "Unsupported 2D primitive topology");
        // Copy the view before return; the GPU resource retains no CPU owner.
        const auto vertices_bytes = static_cast<GLsizeiptr>(vertex_count_ * byte_stride);
        vao_ = create_vertex_array(lifetime);
        glBindVertexArray(vao_.id());
        vbo_[1] = create_buffer(lifetime);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_[1].id());
        glBufferData(GL_ARRAY_BUFFER, vertices_bytes, vertices, GL_STATIC_DRAW);
        RenderAPIs::require_no_gl_error("OpenGL vertex storage upload failed");
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(byte_stride), glBufferOffset<float, 0>());
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(byte_stride), glBufferOffset<float, 3>());
        if (layout_ == Assets::VertexLayout2D::Position3UV2Color4) {
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(byte_stride), glBufferOffset<float, 5>());
        }
        RenderAPIs::require_no_gl_error("OpenGL vertex array layout failed");
    }

    VAO::VAO(
        std::shared_ptr<RenderAPIs::OpenGLResourceLifetime> lifetime,
        std::shared_ptr<Vertex3D> vertices,
        uint32_t num_vertices,
        std::shared_ptr<uint32_t> indices,
        uint32_t num_indices
    )
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
        RenderAPIs::require_no_gl_error("OpenGL index storage upload failed");
        glBindBuffer(GL_ARRAY_BUFFER, vbo_[1].id());
        glBufferData(GL_ARRAY_BUFFER, vertices_bytes, vertices.get(), GL_STATIC_DRAW);
        RenderAPIs::require_no_gl_error("OpenGL vertex storage upload failed");
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, byte_stride, glBufferOffset<float, 0>());
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, byte_stride, glBufferOffset<float, 3>());
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, byte_stride, glBufferOffset<float, 6>());
        RenderAPIs::require_no_gl_error("OpenGL vertex array layout failed");
    }
}
