#include "gl-internal.h"
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

namespace CE::Rendering::OpenGL {
    using namespace ::CE::Rendering::Geometry;

    namespace {
        Handle create_vertex_array(const std::shared_ptr<ResourceLifetime>& lifetime) {
            if (!lifetime)
                throw ::CE::Exceptions::invalid_args(CE_HERE, "OpenGL upload needs a resource lifetime");
            lifetime->require_current();
            ::CE::Rendering::OpenGL::Detail::require_no_gl_error("Cannot create a vertex array with pending OpenGL errors");
            GLuint id = 0;
            glGenVertexArrays(1, &id);
            try {
                ::CE::Rendering::OpenGL::Detail::require_no_gl_error("OpenGL vertex array creation failed");
                return {lifetime, ResourceKind::VertexArray, id};
            } catch (...) {
                lifetime->discard_untracked(ResourceKind::VertexArray, id);
                throw;
            }
        }

        Handle create_buffer(const std::shared_ptr<ResourceLifetime>& lifetime) {
            if (!lifetime)
                throw ::CE::Exceptions::invalid_args(CE_HERE, "OpenGL upload needs a resource lifetime");
            lifetime->require_current();
            ::CE::Rendering::OpenGL::Detail::require_no_gl_error("Cannot create a buffer with pending OpenGL errors");
            GLuint id = 0;
            glGenBuffers(1, &id);
            try {
                ::CE::Rendering::OpenGL::Detail::require_no_gl_error("OpenGL buffer creation failed");
                return {lifetime, ResourceKind::Buffer, id};
            } catch (...) {
                lifetime->discard_untracked(ResourceKind::Buffer, id);
                throw;
            }
        }
    }

    void VertexArray::bind() const {
        glBindVertexArray(vao_.id());
    }

    void VertexArray::require_draw(const std::size_t first_vertex, const std::size_t vertex_count) const {
        (void)vao_.id();
        if (first_vertex > vertex_count_ || vertex_count > vertex_count_ - first_vertex)
            throw ::CE::Exceptions::invalid_args(CE_HERE, "Draw range exceeds uploaded geometry");
        if (type != flat)
            throw ::CE::Exceptions::invalid_args(CE_HERE, "Indexed meshes cannot be drawn as 2D geometry");
    }

    void VertexArray::draw(const std::size_t first_vertex, const std::size_t vertex_count) const {
        require_draw(first_vertex, vertex_count);
        const GLenum mode = topology_ == ::CE::Rendering::Geometry::Topology::TriangleStrip ? GL_TRIANGLE_STRIP : GL_TRIANGLES;
        glDrawArrays(mode, static_cast<GLint>(first_vertex), static_cast<GLsizei>(vertex_count));
    }

    namespace {
        std::span<const ::CE::Rendering::Geometry::Vertex2D>
        vertex_view(const std::shared_ptr<::CE::Rendering::Geometry::Vertex2D>& vertices, const std::uint32_t count) {
            if (!vertices || count == 0)
                throw ::CE::Exceptions::invalid_args(CE_HERE, "Cannot upload empty geometry");
            return {vertices.get(), count};
        }
    }

    VertexArray::VertexArray(
        std::shared_ptr<ResourceLifetime> lifetime,
        std::shared_ptr<::CE::Rendering::Geometry::Vertex2D> vertices,
        const uint32_t num_vertices,
        const ::CE::Rendering::Geometry::Topology topology
    )
    : VertexArray(std::move(lifetime), vertex_view(vertices, num_vertices), topology) {}

    VertexArray::VertexArray(
        std::shared_ptr<ResourceLifetime> lifetime,
        const std::span<const ::CE::Rendering::Geometry::Vertex2D> vertices,
        const ::CE::Rendering::Geometry::Topology topology
    )
    : type(flat), topology_(topology), vertex_count_(vertices.size()) {
        upload_flat(lifetime, vertices.data(), sizeof(::CE::Rendering::Geometry::Vertex2D));
    }

    VertexArray::VertexArray(
        std::shared_ptr<ResourceLifetime> lifetime,
        const std::span<const ::CE::Rendering::Geometry::Vertex2DColor> vertices,
        const ::CE::Rendering::Geometry::Topology topology
    )
    : type(flat), topology_(topology), layout_(::CE::Rendering::Geometry::Layout2D::Position3UV2Color4), vertex_count_(vertices.size()) {
        upload_flat(lifetime, vertices.data(), sizeof(::CE::Rendering::Geometry::Vertex2DColor));
    }

    void VertexArray::upload_flat(const std::shared_ptr<ResourceLifetime>& lifetime, const void* vertices, const std::size_t byte_stride) {
        if (vertex_count_ == 0 || !vertices || vertex_count_ > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()) ||
            vertex_count_ > static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max()) / byte_stride)
            throw ::CE::Exceptions::invalid_args(CE_HERE, "2D geometry exceeds supported buffer/draw sizes");
        if (topology_ != ::CE::Rendering::Geometry::Topology::Triangles && topology_ != ::CE::Rendering::Geometry::Topology::TriangleStrip)
            throw ::CE::Exceptions::invalid_args(CE_HERE, "Unsupported 2D primitive topology");
        // Copy the view before return; the GPU resource retains no CPU owner.
        const auto vertices_bytes = static_cast<GLsizeiptr>(vertex_count_ * byte_stride);
        vao_ = create_vertex_array(lifetime);
        glBindVertexArray(vao_.id());
        vbo_[1] = create_buffer(lifetime);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_[1].id());
        glBufferData(GL_ARRAY_BUFFER, vertices_bytes, vertices, GL_STATIC_DRAW);
        ::CE::Rendering::OpenGL::Detail::require_no_gl_error("OpenGL vertex storage upload failed");
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(byte_stride), glBufferOffset<float, 0>());
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(byte_stride), glBufferOffset<float, 3>());
        if (layout_ == ::CE::Rendering::Geometry::Layout2D::Position3UV2Color4) {
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(byte_stride), glBufferOffset<float, 5>());
        }
        ::CE::Rendering::OpenGL::Detail::require_no_gl_error("OpenGL vertex array layout failed");
    }

    VertexArray::VertexArray(
        std::shared_ptr<ResourceLifetime> lifetime,
        std::shared_ptr<::CE::Rendering::Geometry::Vertex3D> vertices,
        uint32_t num_vertices,
        std::shared_ptr<uint32_t> indices,
        uint32_t num_indices
    )
    : type(mesh), vertex_count_(num_vertices) {
        // Copy mesh indices and interleaved 3D vertices; the CE::Rendering::OpenGL::VertexArray retains the
        // element buffer binding along with position, normal, and UV layout.
        if (!vertices || !indices || num_vertices == 0 || num_indices == 0 ||
            num_vertices > static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max()) / sizeof(::CE::Rendering::Geometry::Vertex3D) ||
            num_indices > static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max()) / sizeof(uint32_t))
            throw ::CE::Exceptions::invalid_args(CE_HERE, "Mesh buffers need nonempty addressable data");
        constexpr GLsizei byte_stride = sizeof(::CE::Rendering::Geometry::Vertex3D);
        const auto vertices_bytes = static_cast<GLsizeiptr>(num_vertices) * sizeof(::CE::Rendering::Geometry::Vertex3D);
        const auto size_vbo_indices = static_cast<GLsizeiptr>(num_indices) * sizeof(uint32_t);
        vao_ = create_vertex_array(lifetime);
        glBindVertexArray(vao_.id());
        vbo_[0] = create_buffer(lifetime);
        vbo_[1] = create_buffer(lifetime);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_[0].id());
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, size_vbo_indices, indices.get(), GL_STATIC_DRAW);
        ::CE::Rendering::OpenGL::Detail::require_no_gl_error("OpenGL index storage upload failed");
        glBindBuffer(GL_ARRAY_BUFFER, vbo_[1].id());
        glBufferData(GL_ARRAY_BUFFER, vertices_bytes, vertices.get(), GL_STATIC_DRAW);
        ::CE::Rendering::OpenGL::Detail::require_no_gl_error("OpenGL vertex storage upload failed");
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, byte_stride, glBufferOffset<float, 0>());
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, byte_stride, glBufferOffset<float, 3>());
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, byte_stride, glBufferOffset<float, 6>());
        ::CE::Rendering::OpenGL::Detail::require_no_gl_error("OpenGL vertex array layout failed");
    }
}
