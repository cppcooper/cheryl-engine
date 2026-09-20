#pragma once

#include <cstdint>
#include <string>

namespace CE {
    struct Vertex2D;

    namespace math {
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

        struct Anchor {
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
