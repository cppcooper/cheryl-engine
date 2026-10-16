#pragma once

#include <assets/types/primitives/vertex.h>

#include <cstdint>
#include <string>

namespace CE {
    namespace Rendering::Geometry {
        /** Normalized origin within a frame: (0,0) is its top-left, (1,1) its bottom-right. */
        struct Pivot {
            float x{0.5f};
            float y{0.5f};

            friend constexpr bool operator==(const Pivot&, const Pivot&) = default;
        };

        enum AnchorType { Center, TopLeft, TopCenter, TopRight, CenterLeft, CenterRight, BottomLeft, BottomCenter, BottomRight };

        // Case-sensitive two-letter names (CL/ML and CR/MR are aliases); unknown
        // names or enum values fall back to Center rather than throwing.
        [[nodiscard]] AnchorType get_anchor(const std::string& anchor);
        [[nodiscard]] Pivot get_pivot(AnchorType anchor);

        /** Convert a top-left-origin image rectangle into local Y-up geometry.
         * The pointer overloads and MakeQuad write six vertices for two independent triangles;
         * MakeQuadStrip produces a separate four-vertex atlas frame.
         * CPU-only: pixel dimensions and offsets, normalized UVs, finite [0,1] pivot
         * and nonzero texture dimensions required. Source rectangle bounds/size are not checked.
         * Pointer destinations must hold six Vertex2D or thirty floats (x,y,z,u,v);
         * they are borrowed through the call. Invalid pivot/texture throws before writes.
         */
        struct Anchor {
            [[nodiscard]] static ::CE::Rendering::Geometry::Quad MakeQuad(
                ::CE::Rendering::Geometry::Pivot pivot,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );
            [[nodiscard]] static ::CE::Rendering::Geometry::QuadTriangleStrip MakeQuadStrip(
                ::CE::Rendering::Geometry::Pivot pivot,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );
            static void MakePivot(
                ::CE::Rendering::Geometry::Pivot pivot,
                float* vertices,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );
            static void MakePivot(
                ::CE::Rendering::Geometry::Pivot pivot,
                ::CE::Rendering::Geometry::Vertex2D* vertices,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );

            static void MakeAnchor(
                ::CE::Rendering::Geometry::AnchorType type,
                float* vertices,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );
            static void MakeAnchor(
                ::CE::Rendering::Geometry::AnchorType type,
                ::CE::Rendering::Geometry::Vertex2D* vertices,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );

            static void Center(
                ::CE::Rendering::Geometry::Vertex2D* vertices,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );
            static void TopLeft(
                ::CE::Rendering::Geometry::Vertex2D* vertices,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );
            static void TopRight(
                ::CE::Rendering::Geometry::Vertex2D* vertices,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );
            static void BottomLeft(
                ::CE::Rendering::Geometry::Vertex2D* vertices,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );
            static void BottomRight(
                ::CE::Rendering::Geometry::Vertex2D* vertices,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );

            static void Center(
                float* vertices,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );
            static void TopLeft(
                float* vertices,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );
            static void TopRight(
                float* vertices,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );
            static void BottomLeft(
                float* vertices,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );
            static void BottomRight(
                float* vertices,
                std::uint32_t texture_width,
                std::uint32_t texture_height,
                std::uint32_t width,
                std::uint32_t height,
                std::uint32_t x0 = 0,
                std::uint32_t y0 = 0
            );
        };
    }
}
