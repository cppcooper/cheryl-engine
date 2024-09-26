#pragma once
#include <string>
#include <cinttypes>

namespace CE {
    class Vertex2D;
}

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
    static void MakeAnchor(AnchorType type, CE::Vertex2D* vertices, uint16_t texture_width, uint16_t texture_height,
                           uint16_t width, uint16_t height, uint16_t x0 = 0, uint16_t y0 = 0
    );
    static void Center(CE::Vertex2D* vertices, uint16_t texture_width, uint16_t texture_height,
                       uint16_t width, uint16_t height, uint16_t x0 = 0, uint16_t y0 = 0
    );
    static void TopLeft(CE::Vertex2D* vertices, uint16_t texture_width, uint16_t texture_height,
                        uint16_t width, uint16_t height, uint16_t x0 = 0, uint16_t y0 = 0
    );
    static void TopRight(CE::Vertex2D* vertices, uint16_t texture_width, uint16_t texture_height,
                         uint16_t width, uint16_t height, uint16_t x0 = 0, uint16_t y0 = 0
    );
    static void BottomLeft(CE::Vertex2D* vertices, uint16_t texture_width, uint16_t texture_height,
                           uint16_t width, uint16_t height, uint16_t x0 = 0, uint16_t y0 = 0
    );
    static void BottomRight(CE::Vertex2D* vertices, uint16_t texture_width, uint16_t texture_height,
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
