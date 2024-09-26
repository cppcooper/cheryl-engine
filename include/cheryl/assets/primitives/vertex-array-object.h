#pragma once
#include <cgl.h>
#include <memory>

namespace CE {
    namespace VAONumbers {
        constexpr GLsizei vertices_per_quad = 4;
        constexpr GLsizei floats_per_quad_vertex = 5;
        constexpr GLsizei floats_per_quad = vertices_per_quad * floats_per_quad_vertex;
        constexpr GLsizei floats_per_mesh_vertex = 8;
        inline GLsizei calculate_num_vertices(GLsizei final_quad) {
            return final_quad * vertices_per_quad;
        }
        inline GLsizei calculate_num_frames(GLsizei num_vertices) {
            return num_vertices / vertices_per_quad;
        }
    }

    struct Vertex2D {
        float x,y,z;
        float u,v;
    };

    struct Vertex3D {
        float x,y,z;
        float nx,ny,nz;
        float u,v;
    };

    struct Quad {
        std::array<Vertex2D,4> vertices;
    };

    struct VAO {
        VAO(std::shared_ptr<Vertex2D> vertices, uint32_t num_vertices);

        VAO(std::shared_ptr<Vertex3D> vertices, uint32_t num_vertices,
            std::shared_ptr<uint32_t> indices, uint32_t num_indices
        );

    protected:
        enum VAOType {
            flat, mesh
        } type;

        GLuint id_vao = 0;
        GLuint id_vbo[2] = {};

    public:
        const GLuint &id = id_vao;
    };
}
