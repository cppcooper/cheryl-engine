#pragma once
#include <string>
#include <cinttypes>

namespace CE {
    class Vertex2D;
    namespace math {
        enum AnchorType {
            Center,
            TopLeft,
            TopRight,
            BottomLeft,
            BottomRight
        };

        inline AnchorType get_anchor(const std::string& anchor) {
            if (anchor == "CE") {
                return AnchorType::Center;
            } else if (anchor == "TL") {
                return AnchorType::TopLeft;
            } else if (anchor == "TR") {
                return AnchorType::TopRight;
            } else if (anchor == "BL") {
                return AnchorType::BottomLeft;
            } else if (anchor == "BR") {
                return AnchorType::BottomRight;
            }
            return AnchorType::Center;
        }

        struct Anchor {
            static void MakeAnchor(AnchorType type, float* vertices, uint16_t texture_width, uint16_t texture_height,
                                   uint16_t width, uint16_t height, uint16_t x0 = 0, uint16_t y0 = 0
            );
            static void MakeAnchor(AnchorType type, Vertex2D* vertices, uint16_t texture_width, uint16_t texture_height,
                                   uint16_t width, uint16_t height, uint16_t x0 = 0, uint16_t y0 = 0
            );
            static void Center(Vertex2D* vertices, uint16_t texture_width, uint16_t texture_height,
                               uint16_t width, uint16_t height, uint16_t x0 = 0, uint16_t y0 = 0
            );
            static void TopLeft(Vertex2D* vertices, uint16_t texture_width, uint16_t texture_height,
                                uint16_t width, uint16_t height, uint16_t x0 = 0, uint16_t y0 = 0
            );
            static void TopRight(Vertex2D* vertices, uint16_t texture_width, uint16_t texture_height,
                                 uint16_t width, uint16_t height, uint16_t x0 = 0, uint16_t y0 = 0
            );
            static void BottomLeft(Vertex2D* vertices, uint16_t texture_width, uint16_t texture_height,
                                   uint16_t width, uint16_t height, uint16_t x0 = 0, uint16_t y0 = 0
            );
            static void BottomRight(Vertex2D* vertices, uint16_t texture_width, uint16_t texture_height,
                                    uint16_t width, uint16_t height, uint16_t x0 = 0, uint16_t y0 = 0
            );
            static void Center(float* vertices, uint16_t texture_width, uint16_t texture_height,
                               uint16_t width, uint16_t height, uint16_t x0 = 0, uint16_t y0 = 0
            );
            static void TopLeft(float* vertices, uint16_t texture_width, uint16_t texture_height,
                                uint16_t width, uint16_t height, uint16_t x0 = 0, uint16_t y0 = 0
            );
            static void TopRight(float* vertices, uint16_t texture_width, uint16_t texture_height,
                                 uint16_t width, uint16_t height, uint16_t x0 = 0, uint16_t y0 = 0
            );
            static void BottomLeft(float* vertices, uint16_t texture_width, uint16_t texture_height,
                                   uint16_t width, uint16_t height, uint16_t x0 = 0, uint16_t y0 = 0
            );
            static void BottomRight(float* vertices, uint16_t texture_width, uint16_t texture_height,
                                    uint16_t width, uint16_t height, uint16_t x0 = 0, uint16_t y0 = 0
            );
        };
    }
}


