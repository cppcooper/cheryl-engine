#pragma once

#include <cheryl/export.h>

#include <array>
#include <assets/resources/geometry2d.h>
#include <assets/types/primitives/vertex.h>
#include <backends/opengl/gl.h>
#include <backends/opengl/resource-lifetime.h>
#include <cstdint>
#include <memory>
#include <span>
#include <cstddef>

namespace CE::Rendering::OpenGL {
    struct CE_TYPE_API VertexArray final : ::CE::Rendering::Geometry::Buffer2D {
    protected:
        enum VAOType { flat, mesh } type;

        ::CE::Rendering::Geometry::Topology topology_ = ::CE::Rendering::Geometry::Topology::Triangles;
        ::CE::Rendering::Geometry::Layout2D layout_ = ::CE::Rendering::Geometry::Layout2D::Position3UV2;
        std::size_t vertex_count_ = 0;

        ::CE::Rendering::OpenGL::Handle vao_;
        std::array<::CE::Rendering::OpenGL::Handle, 2> vbo_;

    public:
        CE_OPENGL_API VertexArray(
            std::shared_ptr<::CE::Rendering::OpenGL::ResourceLifetime> lifetime,
            std::span<const ::CE::Rendering::Geometry::Vertex2D> vertices,
            ::CE::Rendering::Geometry::Topology topology
        );
        CE_OPENGL_API VertexArray(
            std::shared_ptr<::CE::Rendering::OpenGL::ResourceLifetime> lifetime,
            std::span<const ::CE::Rendering::Geometry::Vertex2DColor> vertices,
            ::CE::Rendering::Geometry::Topology topology
        );
        CE_OPENGL_API VertexArray(
            std::shared_ptr<::CE::Rendering::OpenGL::ResourceLifetime> lifetime,
            std::shared_ptr<::CE::Rendering::Geometry::Vertex2D> vertices,
            uint32_t num_vertices,
            ::CE::Rendering::Geometry::Topology topology
        );

        CE_OPENGL_API VertexArray(
            std::shared_ptr<::CE::Rendering::OpenGL::ResourceLifetime> lifetime,
            std::shared_ptr<::CE::Rendering::Geometry::Vertex3D> vertices,
            uint32_t num_vertices,
            std::shared_ptr<uint32_t> indices,
            uint32_t num_indices
        );

        CE_OPENGL_API void bind() const override;
        CE_OPENGL_API void draw(std::size_t first_vertex, std::size_t vertex_count) const override;
        [[nodiscard]] ::CE::Rendering::Geometry::Layout2D vertex_layout() const noexcept override {
            return type == flat ? layout_ : ::CE::Rendering::Geometry::Layout2D::Unsupported;
        }
        [[nodiscard]] ::CE::Rendering::Geometry::Topology topology() const noexcept override { return topology_; }
        [[nodiscard]] std::size_t vertex_count() const noexcept override { return vertex_count_; }
        [[nodiscard]] const ::CE::Rendering::OpenGL::ResourceLifetime* resource_domain() const noexcept { return vao_.resource_domain(); }
        CE_OPENGL_API void require_draw(std::size_t first_vertex, std::size_t vertex_count) const;

    private:
        CE_OPENGL_API void upload_flat(
            const std::shared_ptr<::CE::Rendering::OpenGL::ResourceLifetime>& lifetime,
            const void* vertices,
            std::size_t byte_stride
        );
    };
}
