#pragma once

#include <array>
#include <cstddef>
#include <stdexcept>

namespace CE {
    namespace VAONumbers {
        // A standalone quad has two independent triangles. Atlas cells use strips instead.
        constexpr int vertices_per_quad = 6;
        constexpr int vertices_per_strip_quad = 4;
        constexpr int floats_per_quad_vertex = 5;
        constexpr int floats_per_quad = vertices_per_quad * floats_per_quad_vertex;
        constexpr int floats_per_mesh_vertex = 8;

        inline int calculate_num_vertices(const int final_quad) {
            return final_quad * vertices_per_quad;
        }

        inline int calculate_num_strip_vertices(const int final_quad) {
            return final_quad * vertices_per_strip_quad;
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

    /** A single face with its vertices in counterclockwise order. */
    struct Triangle {
        std::array<Vertex2D, 3> vertices;
    };

    /** Adjacent faces share two vertices; the face winding alternates along the strip. */
    template <std::size_t TriangleCount>
    struct TriangleStrip {
        static_assert(TriangleCount > 0);
        std::array<Vertex2D, TriangleCount + 2> vertices;

        constexpr TriangleStrip(const Triangle& first, const std::array<Vertex2D, TriangleCount - 1>& remaining)
            : vertices{} {
            for (std::size_t i = 0; i < first.vertices.size(); ++i) vertices[i] = first.vertices[i];
            for (std::size_t i = 0; i < remaining.size(); ++i) vertices[i + 3] = remaining[i];
        }

        [[nodiscard]] constexpr Triangle triangle(const std::size_t index) const {
            if (index >= TriangleCount) throw std::out_of_range("Triangle lies outside the strip");
            const auto& tip = vertices[index + 2];
            if (index % 2 == 0) return {{vertices[index], vertices[index + 1], tip}};
            return {{vertices[index + 1], vertices[index], tip}};
        }
    };

    /** Two independent triangles for a standalone textured image or graphic. */
    struct Quad {
        std::array<Vertex2D, VAONumbers::vertices_per_quad> vertices;
    };

    /** A four-corner strip for one independently drawn atlas cell. */
    struct QuadTriangleStrip {
        TriangleStrip<2> triangles;

        constexpr QuadTriangleStrip(const Triangle& first, const Vertex2D& fourth)
            : triangles(first, {fourth}) {}

        [[nodiscard]] constexpr const std::array<Vertex2D, VAONumbers::vertices_per_strip_quad>& vertices() const {
            return triangles.vertices;
        }
    };
}
