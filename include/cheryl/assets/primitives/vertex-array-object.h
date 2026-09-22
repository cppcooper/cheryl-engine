#pragma once
#include "vertex.h"
#include <cgl.h>
#include <memory>
#include <cstdint>

namespace CE {
    struct VAO {
        VAO(std::shared_ptr<Vertex2D> vertices, uint32_t num_vertices);

        VAO(std::shared_ptr<Vertex3D> vertices, uint32_t num_vertices,
            std::shared_ptr<uint32_t> indices, uint32_t num_indices
        );

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
