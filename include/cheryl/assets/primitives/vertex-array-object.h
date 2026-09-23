#pragma once
#include "vertex.h"
#include <assets/abstracts/geometry2d.h>
#include <cgl.h>
#include <memory>
#include <cstdint>

namespace CE {
    struct VAO final : Assets::Geometry2D {
        VAO(std::shared_ptr<Vertex2D> vertices, uint32_t num_vertices);

        VAO(std::shared_ptr<Vertex3D> vertices, uint32_t num_vertices,
            std::shared_ptr<uint32_t> indices, uint32_t num_indices
        );

        void bind(const Assets::Image& image) const override;
        void draw(std::size_t first_vertex, std::size_t vertex_count) const override;

    protected:
        enum VAOType {
            flat, mesh
        } type;

        // TODO: Coordinate glDeleteVertexArrays/glDeleteBuffers and Texture's glDeleteTextures with context lifetime.
        GLuint id_vao = 0;
        GLuint id_vbo[2] = {};

    public:
        const GLuint &id = id_vao;
    };
}
