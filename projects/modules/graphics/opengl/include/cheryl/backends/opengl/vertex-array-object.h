#pragma once

#include <array>
#include <assets/resources/geometry2d.h>
#include <assets/types/primitives/vertex.h>
#include <backends/opengl/gl.h>
#include <backends/opengl/resource-lifetime.h>
#include <cstdint>
#include <memory>
#include <span>
#include <cstddef>

namespace CE {
    struct VAO final : Assets::Geometry2D {
    protected:
        enum VAOType { flat, mesh } type;

        Assets::PrimitiveTopology topology_ = Assets::PrimitiveTopology::Triangles;
        Assets::VertexLayout2D layout_ = Assets::VertexLayout2D::Position3UV2;
        std::size_t vertex_count_ = 0;

        RenderAPIs::OpenGLHandle vao_;
        std::array<RenderAPIs::OpenGLHandle, 2> vbo_;

    public:
        VAO(std::shared_ptr<RenderAPIs::OpenGLResourceLifetime> lifetime,
            std::span<const Vertex2D> vertices,
            Assets::PrimitiveTopology topology);
        VAO(std::shared_ptr<RenderAPIs::OpenGLResourceLifetime> lifetime,
            std::span<const Vertex2DColor> vertices,
            Assets::PrimitiveTopology topology);
        VAO(std::shared_ptr<RenderAPIs::OpenGLResourceLifetime> lifetime,
            std::shared_ptr<Vertex2D> vertices,
            uint32_t num_vertices,
            Assets::PrimitiveTopology topology);

        VAO(std::shared_ptr<RenderAPIs::OpenGLResourceLifetime> lifetime,
            std::shared_ptr<Vertex3D> vertices,
            uint32_t num_vertices,
            std::shared_ptr<uint32_t> indices,
            uint32_t num_indices);

        void bind() const override;
        void draw(std::size_t first_vertex, std::size_t vertex_count) const override;
        [[nodiscard]] Assets::VertexLayout2D vertex_layout() const noexcept override {
            return type == flat ? layout_ : Assets::VertexLayout2D::Unsupported;
        }
        [[nodiscard]] Assets::PrimitiveTopology topology() const noexcept override { return topology_; }
        [[nodiscard]] std::size_t vertex_count() const noexcept override { return vertex_count_; }
        [[nodiscard]] const RenderAPIs::OpenGLResourceLifetime* resource_domain() const noexcept { return vao_.resource_domain(); }
        void require_draw(std::size_t first_vertex, std::size_t vertex_count) const;

    private:
        void upload_flat(
            const std::shared_ptr<RenderAPIs::OpenGLResourceLifetime>& lifetime,
            const void* vertices,
            std::size_t byte_stride
        );
    };
}
