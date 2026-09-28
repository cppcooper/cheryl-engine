#pragma once

#include <assets/types/primitives/vertex.h>

#include <cstdint>
#include <string>

namespace CE {
    namespace math {
        /** Normalized origin within a frame: (0,0) is its top-left, (1,1) its bottom-right. */
        struct Pivot {
            float x{0.5f};
            float y{0.5f};

            friend constexpr bool operator==(const Pivot&, const Pivot&) = default;
        };

        enum AnchorType {
            Center,
            TopLeft,
            TopCenter,
            TopRight,
            CenterLeft,
            CenterRight,
            BottomLeft,
            BottomCenter,
            BottomRight
        };

        [[nodiscard]] AnchorType get_anchor(const std::string& anchor);
        [[nodiscard]] Pivot get_pivot(AnchorType anchor);

        /** Convert a top-left-origin image rectangle into local Y-up geometry.
         * The pointer overloads and MakeQuad write six vertices for two independent triangles;
         * MakeQuadStrip produces a separate four-vertex atlas frame.
         */
        struct Anchor {
            [[nodiscard]] static Quad MakeQuad(Pivot pivot, std::uint32_t texture_width,
                                               std::uint32_t texture_height, std::uint32_t width,
                                               std::uint32_t height, std::uint32_t x0 = 0, std::uint32_t y0 = 0);
            [[nodiscard]] static QuadTriangleStrip MakeQuadStrip(Pivot pivot, std::uint32_t texture_width,
                                                                 std::uint32_t texture_height, std::uint32_t width,
                                                                 std::uint32_t height, std::uint32_t x0 = 0,
                                                                 std::uint32_t y0 = 0);
            static void MakePivot(Pivot pivot, float* vertices, std::uint32_t texture_width,
                                  std::uint32_t texture_height, std::uint32_t width, std::uint32_t height,
                                  std::uint32_t x0 = 0, std::uint32_t y0 = 0);
            static void MakePivot(Pivot pivot, Vertex2D* vertices, std::uint32_t texture_width,
                                  std::uint32_t texture_height, std::uint32_t width, std::uint32_t height,
                                  std::uint32_t x0 = 0, std::uint32_t y0 = 0);

            static void MakeAnchor(AnchorType type, float* vertices, std::uint32_t texture_width,
                                   std::uint32_t texture_height, std::uint32_t width, std::uint32_t height,
                                   std::uint32_t x0 = 0, std::uint32_t y0 = 0);
            static void MakeAnchor(AnchorType type, Vertex2D* vertices, std::uint32_t texture_width,
                                   std::uint32_t texture_height, std::uint32_t width, std::uint32_t height,
                                   std::uint32_t x0 = 0, std::uint32_t y0 = 0);

            static void Center(Vertex2D* vertices, std::uint32_t texture_width, std::uint32_t texture_height,
                               std::uint32_t width, std::uint32_t height, std::uint32_t x0 = 0, std::uint32_t y0 = 0);
            static void TopLeft(Vertex2D* vertices, std::uint32_t texture_width, std::uint32_t texture_height,
                                std::uint32_t width, std::uint32_t height, std::uint32_t x0 = 0, std::uint32_t y0 = 0);
            static void TopRight(Vertex2D* vertices, std::uint32_t texture_width, std::uint32_t texture_height,
                                 std::uint32_t width, std::uint32_t height, std::uint32_t x0 = 0, std::uint32_t y0 = 0);
            static void BottomLeft(Vertex2D* vertices, std::uint32_t texture_width, std::uint32_t texture_height,
                                   std::uint32_t width, std::uint32_t height, std::uint32_t x0 = 0,
                                   std::uint32_t y0 = 0);
            static void BottomRight(Vertex2D* vertices, std::uint32_t texture_width, std::uint32_t texture_height,
                                    std::uint32_t width, std::uint32_t height, std::uint32_t x0 = 0,
                                    std::uint32_t y0 = 0);

            static void Center(float* vertices, std::uint32_t texture_width, std::uint32_t texture_height,
                               std::uint32_t width, std::uint32_t height, std::uint32_t x0 = 0, std::uint32_t y0 = 0);
            static void TopLeft(float* vertices, std::uint32_t texture_width, std::uint32_t texture_height,
                                std::uint32_t width, std::uint32_t height, std::uint32_t x0 = 0, std::uint32_t y0 = 0);
            static void TopRight(float* vertices, std::uint32_t texture_width, std::uint32_t texture_height,
                                 std::uint32_t width, std::uint32_t height, std::uint32_t x0 = 0, std::uint32_t y0 = 0);
            static void BottomLeft(float* vertices, std::uint32_t texture_width, std::uint32_t texture_height,
                                   std::uint32_t width, std::uint32_t height, std::uint32_t x0 = 0,
                                   std::uint32_t y0 = 0);
            static void BottomRight(float* vertices, std::uint32_t texture_width, std::uint32_t texture_height,
                                    std::uint32_t width, std::uint32_t height, std::uint32_t x0 = 0,
                                    std::uint32_t y0 = 0);
        };
    }
}
