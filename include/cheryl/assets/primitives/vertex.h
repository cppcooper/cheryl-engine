#pragma once

#include <array>

namespace CE {
    namespace VAONumbers {
        constexpr int vertices_per_quad = 6;
        constexpr int floats_per_quad_vertex = 5;
        constexpr int floats_per_quad = vertices_per_quad * floats_per_quad_vertex;
        constexpr int floats_per_mesh_vertex = 8;

        inline int calculate_num_vertices(const int final_quad) {
            return final_quad * vertices_per_quad;
        }

        inline int calculate_num_frames(const int num_vertices) {
            return num_vertices / vertices_per_quad;
        }
    }

    struct Vertex2D {
        float x, y, z;
        float u, v;
    };

    struct Vertex3D {
        float x, y, z;
        float nx, ny, nz;
        float u, v;
    };

    struct Quad {
        std::array<Vertex2D, VAONumbers::vertices_per_quad> vertices;
    };
}
