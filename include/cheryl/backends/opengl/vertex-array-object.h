#pragma once

#include <array>
#include <assets/resources/geometry2d.h>
#include <assets/types/primitives/vertex.h>
#include <backends/opengl/gl.h>
#include <backends/opengl/resource-lifetime.h>
#include <cstdint>
#include <memory>
#include <span>

namespace CE {
    struct VAO final : Assets::Geometry2D {
        VAO(std::shared_ptr<RenderAPIs::OpenGLResourceLifetime> lifetime,
            std::span<const Vertex2D> vertices,
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

        void bind(const Assets::Image& image) const override;
        void draw(std::size_t first_vertex, std::size_t vertex_count) const override;

    protected:
        enum VAOType { flat, mesh } type;

        Assets::PrimitiveTopology topology_ = Assets::PrimitiveTopology::Triangles;
        std::size_t vertex_count_ = 0;

        RenderAPIs::OpenGLHandle vao_;
        std::array<RenderAPIs::OpenGLHandle, 2> vbo_;
    };
}
