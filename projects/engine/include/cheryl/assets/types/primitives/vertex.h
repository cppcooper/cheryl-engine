#pragma once

#include <array>
#include <cstddef>
#include <stdexcept>

namespace CE::Rendering::Geometry {
    // A standalone quad has two independent triangles. Atlas cells use strips instead.
    constexpr int vertices_per_quad = 6; ///< Number of vertices in two independent triangles for a standalone quad.
    constexpr int vertices_per_strip_quad = 4; ///< Number of vertices in one independently drawn four-corner atlas strip.
    constexpr int floats_per_quad_vertex = 5; ///< Five interleaved components x,y,z,u,v in the uncolored 2D layout.
    constexpr int floats_per_quad = vertices_per_quad * floats_per_quad_vertex; ///< Float count in one standalone six-vertex uncolored quad.
    constexpr int floats_per_mesh_vertex = 8; ///< Eight interleaved position/normal/UV components in Vertex3D; no mesh renderer is implied.

    inline int calculate_num_vertices(const int final_quad) {
        return final_quad * vertices_per_quad;
    }

    inline int calculate_num_strip_vertices(const int final_quad) {
        return final_quad * vertices_per_strip_quad;
    }

    inline int calculate_num_frames(const int num_vertices) {
        return num_vertices / vertices_per_quad;
    }

/** CPU position and texture coordinates; the consuming pipeline defines transform/texture semantics. */
    struct Vertex2D {
        float x, y, z;
        float u, v;
    };

    // Float RGBA values consumed by the selected pipeline's shader and blend mode.
    // Colors are copied with geometry, without changing the existing Vertex2D layout.
/** CPU position/UV plus float RGBA; shader/blend semantics decide interpretation and premultiplication. */
    struct Vertex2DColor {
        float x, y, z;
        float u, v;
        float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
    };

/** CPU position, normal and UV data only; this does not implement mesh/model rendering. */
    struct Vertex3D {
        float x, y, z;
        float nx, ny, nz;
        float u, v;
    };

    /** A single face with its vertices in counterclockwise order. */
    struct Triangle {
        std::array<Vertex2D, 3> vertices; ///< Owned counterclockwise triangle vertices.
    };

    /** Adjacent faces share two vertices; the face winding alternates along the strip. */
    template <std::size_t TriangleCount> struct TriangleStrip {
        static_assert(TriangleCount > 0);
        std::array<Vertex2D, TriangleCount + 2> vertices; ///< Owned strip order; the first three define the initial face.

        constexpr TriangleStrip(const Triangle& first, const std::array<Vertex2D, TriangleCount - 1>& remaining)
        : vertices{} {
            for (std::size_t i = 0; i < first.vertices.size(); ++i)
                vertices[i] = first.vertices[i];
            for (std::size_t i = 0; i < remaining.size(); ++i)
                vertices[i + 3] = remaining[i];
        }

        [[nodiscard]] constexpr Triangle triangle(const std::size_t index) const {
            if (index >= TriangleCount)
                throw std::out_of_range("Triangle lies outside the strip");
            const auto& tip = vertices[index + 2];
            if (index % 2 == 0)
                return {{vertices[index], vertices[index + 1], tip}};
            return {{vertices[index + 1], vertices[index], tip}};
        }
    };

    /** Two independent triangles for a standalone textured image or graphic. */
    struct Quad {
        std::array<Vertex2D, CE::Rendering::Geometry::vertices_per_quad> vertices; ///< Six owned vertices, two independent triangles.
    };

    /** A four-corner strip for one independently drawn atlas cell. */
    struct QuadTriangleStrip {
        TriangleStrip<2> triangles; ///< Two-face strip retaining its four vertices.

        constexpr QuadTriangleStrip(const Triangle& first, const Vertex2D& fourth)
        : triangles(first, {fourth}) {}

        [[nodiscard]] constexpr const std::array<Vertex2D, CE::Rendering::Geometry::vertices_per_strip_quad>& vertices() const {
            return triangles.vertices;
        }
    };
}

/**
 * @fn int CE::Rendering::Geometry::calculate_num_vertices(int final_quad)
 * @brief Multiply a quad count by the standalone six-vertex layout.
 * @param final_quad Caller-validated count whose product fits int; no overflow/sign validation occurs.
 * @return final_quad * vertices_per_quad, not a byte count.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#coordinates-and-grid-geometry)
 */

/**
 * @fn int CE::Rendering::Geometry::calculate_num_strip_vertices(int final_quad)
 * @brief Multiply a cell/quad count by the four-vertex strip layout.
 * @param final_quad Caller-validated count whose product fits int; no validation occurs.
 * @return final_quad * vertices_per_strip_quad; independently drawn cells do not share strip boundaries.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#coordinates-and-grid-geometry)
 */

/**
 * @fn int CE::Rendering::Geometry::calculate_num_frames(int num_vertices)
 * @brief Convert a vertex count using the standalone six-vertex quad layout.
 * @param num_vertices Caller-validated nonnegative count.
 * @return Integer quotient by vertices_per_quad; an incomplete trailing quad is truncated. This is not the strip-cell formula.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#coordinates-and-grid-geometry)
 */

/**
 * @fn template<std::size_t TriangleCount> constexpr CE::Rendering::Geometry::TriangleStrip<TriangleCount>::TriangleStrip(const Triangle& first, const std::array<Vertex2D, TriangleCount - 1>& remaining)
 * @brief Copy the first face and one additional vertex per subsequent strip face.
 * @tparam TriangleCount Positive compile-time face count.
 * @param first Initial counterclockwise triangle.
 * @param remaining Subsequent strip vertices in order; no geometry/winding validation occurs.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#coordinates-and-grid-geometry)
 */

/**
 * @fn template<std::size_t TriangleCount> constexpr CE::Rendering::Geometry::Triangle CE::Rendering::Geometry::TriangleStrip<TriangleCount>::triangle(std::size_t index) const
 * @brief Copy one face while correcting alternating strip winding.
 * @param index Zero-based face below TriangleCount.
 * @return Counterclockwise face when the input strip follows its declared winding.
 * @throws std::out_of_range For an index outside the strip.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#coordinates-and-grid-geometry)
 */

/**
 * @fn constexpr CE::Rendering::Geometry::QuadTriangleStrip::QuadTriangleStrip(const Triangle& first, const Vertex2D& fourth)
 * @brief Copy the initial triangle and fourth corner into a two-face strip.
 * Inputs are retained by value, not borrowed; geometry is not uploaded.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#coordinates-and-grid-geometry)
 */

/**
 * @fn constexpr const std::array<CE::Rendering::Geometry::Vertex2D, CE::Rendering::Geometry::vertices_per_strip_quad>& CE::Rendering::Geometry::QuadTriangleStrip::vertices() const
 * @brief Borrow the four owned strip vertices for this value's lifetime.
 * The array is not a six-vertex independent-triangle quad; use the matching topology.
 * @see [Detailed guide](cheryl-guide:assets/asset-values-and-playback#coordinates-and-grid-geometry)
 */

/** @var float CE::Rendering::Geometry::Vertex2D::x
 * Horizontal local position; units follow the consuming geometry API. Values are stored without validation.
 */

/** @var float CE::Rendering::Geometry::Vertex2D::y
 * Vertical local position, Y-up for Anchor/grid builders. Values are stored without validation.
 */

/** @var float CE::Rendering::Geometry::Vertex2D::z
 * Local depth coordinate. Values are stored without validation.
 */

/** @var float CE::Rendering::Geometry::Vertex2D::u
 * Normalized horizontal texture coordinate for standard image pipelines. Values are stored without validation.
 */

/** @var float CE::Rendering::Geometry::Vertex2D::v
 * Normalized vertical texture coordinate; standard builders convert top-left pixels to bottom-left UV origin. Values are stored without validation.
 */

/** @var float CE::Rendering::Geometry::Vertex2DColor::x
 * Horizontal local position. Values are stored without validation.
 */

/** @var float CE::Rendering::Geometry::Vertex2DColor::y
 * Vertical local position. Values are stored without validation.
 */

/** @var float CE::Rendering::Geometry::Vertex2DColor::z
 * Local depth coordinate. Values are stored without validation.
 */

/** @var float CE::Rendering::Geometry::Vertex2DColor::u
 * Horizontal texture coordinate. Values are stored without validation.
 */

/** @var float CE::Rendering::Geometry::Vertex2DColor::v
 * Vertical texture coordinate. Values are stored without validation.
 */

/** @var float CE::Rendering::Geometry::Vertex2DColor::r
 * Red float component, default1; interpretation follows the selected shader. Values are stored without validation.
 */

/** @var float CE::Rendering::Geometry::Vertex2DColor::g
 * Green float component, default1. Values are stored without validation.
 */

/** @var float CE::Rendering::Geometry::Vertex2DColor::b
 * Blue float component, default1. Values are stored without validation.
 */

/** @var float CE::Rendering::Geometry::Vertex2DColor::a
 * Alpha float component, default1; no implicit premultiplication is applied here. Values are stored without validation.
 */

/** @var float CE::Rendering::Geometry::Vertex3D::x
 * Local position X. Values are stored without validation.
 */

/** @var float CE::Rendering::Geometry::Vertex3D::y
 * Local position Y. Values are stored without validation.
 */

/** @var float CE::Rendering::Geometry::Vertex3D::z
 * Local position Z. Values are stored without validation.
 */

/** @var float CE::Rendering::Geometry::Vertex3D::nx
 * Normal X; caller/pipeline defines normalization requirements. Values are stored without validation.
 */

/** @var float CE::Rendering::Geometry::Vertex3D::ny
 * Normal Y. Values are stored without validation.
 */

/** @var float CE::Rendering::Geometry::Vertex3D::nz
 * Normal Z. Values are stored without validation.
 */

/** @var float CE::Rendering::Geometry::Vertex3D::u
 * Texture coordinate U. Values are stored without validation.
 */

/** @var float CE::Rendering::Geometry::Vertex3D::v
 * Texture coordinate V. Values are stored without validation.
 */
