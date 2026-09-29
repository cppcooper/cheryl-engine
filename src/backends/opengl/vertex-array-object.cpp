#include <backends/opengl/vertex-array-object.h>
#include <backends/opengl/texture.h>

#include <internals/exceptions.h>

template<typename T, uint64_t offset>
constexpr uint64_t get_length() {
    return sizeof(T) * offset;
}

template<typename T, uint64_t offset>
constexpr void* glBufferOffset() {
    return (void*)get_length<T, offset>();
}
namespace CE {
    using namespace VAONumbers;

    namespace {
        RenderAPIs::OpenGLHandle create_vertex_array(
            const std::shared_ptr<RenderAPIs::OpenGLResourceLifetime>& lifetime) {
            GLuint id = 0;
            glGenVertexArrays(1, &id);
            try {
                return {lifetime, RenderAPIs::GLResourceKind::VertexArray, id};
            } catch (...) {
                if (id) glDeleteVertexArrays(1, &id);
                throw;
            }
        }

        RenderAPIs::OpenGLHandle create_buffer(
            const std::shared_ptr<RenderAPIs::OpenGLResourceLifetime>& lifetime) {
            GLuint id = 0;
            glGenBuffers(1, &id);
            try {
                return {lifetime, RenderAPIs::GLResourceKind::Buffer, id};
            } catch (...) {
                if (id) glDeleteBuffers(1, &id);
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
        if (type != flat)
            throw Exceptions::invalid_args(CE_HERE, "Indexed meshes cannot be drawn as 2D geometry");
        const GLenum mode = topology_ == Assets::PrimitiveTopology::TriangleStrip ? GL_TRIANGLE_STRIP : GL_TRIANGLES;
        glDrawArrays(mode, static_cast<GLint>(first_vertex), static_cast<GLsizei>(vertex_count));
    }

    VAO::VAO(std::shared_ptr<RenderAPIs::OpenGLResourceLifetime> lifetime,
             std::shared_ptr<Vertex2D> vertices, uint32_t num_vertices, const Assets::PrimitiveTopology topology)
            : type(flat), topology_(topology) {
        // Copy 2D vertices to the GPU, then record the position/UV layout
        // in the VAO. The caller's CPU vertex buffer can be released after upload.
        constexpr GLsizei byte_stride = sizeof(Vertex2D);
        const GLsizei vertices_bytes = num_vertices * byte_stride;
        vao_ = create_vertex_array(lifetime);
        glBindVertexArray(vao_.id());
        vbo_[1] = create_buffer(lifetime);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_[1].id());
        glBufferData(GL_ARRAY_BUFFER, vertices_bytes, vertices.get(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, byte_stride, glBufferOffset<float, 0>());
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, byte_stride, glBufferOffset<float, 3>());
    }

    VAO::VAO(std::shared_ptr<RenderAPIs::OpenGLResourceLifetime> lifetime,
             std::shared_ptr<Vertex3D> vertices, uint32_t num_vertices,
             std::shared_ptr<uint32_t> indices, uint32_t num_indices
    ) : type(mesh) {
        // Copy mesh indices and interleaved 3D vertices; the VAO retains the
        // element buffer binding along with position, normal, and UV layout.
        constexpr GLsizei byte_stride = sizeof(Vertex3D);
        const GLsizei vertices_bytes = num_vertices * byte_stride;
        const GLsizei size_vbo_indices = num_indices * sizeof(uint32_t);
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
