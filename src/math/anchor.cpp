#include <assets/primitives/vertex-array-object.h>
#include <math/anchor.h>

void Anchor::MakeAnchor(AnchorType type, float* vertices, uint16_t texture_width, uint16_t texture_height,
                        uint16_t width, uint16_t height, uint16_t x0, uint16_t y0
) {
    switch (type) {
        case ::Center:
            Center(vertices, texture_width, texture_height, width, height, x0, y0);
            break;
        case ::TopLeft:
            TopLeft(vertices, texture_width, texture_height, width, height, x0, y0);
            break;
        case ::TopRight:
            TopRight(vertices, texture_width, texture_height, width, height, x0, y0);
            break;
        case ::BottomLeft:
            BottomLeft(vertices, texture_width, texture_height, width, height, x0, y0);
            break;
        case ::BottomRight:
            BottomRight(vertices, texture_width, texture_height, width, height, x0, y0);
            break;
    }
}

void Anchor::MakeAnchor(AnchorType type, CE::Vertex2D* vertices, uint16_t texture_width, uint16_t texture_height,
                        uint16_t width, uint16_t height, uint16_t x0, uint16_t y0
) {
    switch (type) {
        case ::Center:
            Center(vertices, texture_width, texture_height, width, height, x0, y0);
        break;
        case ::TopLeft:
            TopLeft(vertices, texture_width, texture_height, width, height, x0, y0);
        break;
        case ::TopRight:
            TopRight(vertices, texture_width, texture_height, width, height, x0, y0);
        break;
        case ::BottomLeft:
            BottomLeft(vertices, texture_width, texture_height, width, height, x0, y0);
        break;
        case ::BottomRight:
            BottomRight(vertices, texture_width, texture_height, width, height, x0, y0);
        break;
    }
}

inline void CalcUVs(float* vertices, float twidth, float theight, float fwidth, float fheight, float sx, float sy) {
    ///Bottom Left
    vertices[2] = 0.f;
    vertices[3] = sx / twidth;                        // U - Texture mapping
    vertices[4] = 1.0f-((sy+fheight) / theight);      // V - Texture mapping (Inverted Axis?)

    ///Bottom Right
    vertices[7] = 0.0f;
    vertices[8] = (sx+fwidth) / twidth;
    vertices[9] = 1.0f-((sy+fheight) / theight);

    ///Top Right
    vertices[12] = 0.0f;
    vertices[13] = (sx+fwidth) / twidth;
    vertices[14] = 1.0f-(sy / theight);

    ///Top Left
    vertices[17] = 0.0f;
    vertices[18] = sx / twidth;
    vertices[19] = 1.0f-(sy / theight);
}

void Anchor::Center(CE::Vertex2D* vertices, uint16_t texture_width, uint16_t texture_height,
                    uint16_t width, uint16_t height, uint16_t x0, uint16_t y0
) {
    float twidth = static_cast<float>(texture_width);
    float theight = static_cast<float>(texture_height);
    float fwidth = static_cast<float>(width);
    float fheight = static_cast<float>(height);
    float hfw = fwidth / 2.f;
    float hfh = fheight / 2.f;
    // Top Left
    vertices[0] = {
        0-hfw,
        0-hfh,
        0,
        x0 / twidth,
        1.0f-((y0+fheight) / theight)
    };
    // Bottom Right
    vertices[1] = {
        0+hfw,
        0-hfh,
        0,
        (x0+fwidth) / twidth,
        1.0f-((y0+fheight) / theight)
    };
    // Top Right
    vertices[2] = {
        0+hfw,
        0+hfh,
        0,
        (x0+fwidth) / twidth,
        1.0f-(y0 / theight)
    };
    // Top Left
    vertices[3] = {
        0-hfw,
        0+hfh,
        0,
        x0 / twidth,
        1.0f-(y0 / theight)
    };
}

void Anchor::TopLeft(CE::Vertex2D* vertices, uint16_t texture_width, uint16_t texture_height,
                    uint16_t width, uint16_t height, uint16_t x0, uint16_t y0
) {
    float twidth = static_cast<float>(texture_width);
    float theight = static_cast<float>(texture_height);
    float fwidth = static_cast<float>(width);
    float fheight = static_cast<float>(height);
    // Bottom Left
    vertices[0] = {
        0,
        -fheight,
        0,
        x0 / twidth,
        1.0f-((y0+fheight) / theight)
    };
    // Bottom Right
    vertices[1] = {
        fwidth,
        -fheight,
        0,
        (x0+fwidth) / twidth,
        1.0f-((y0+fheight) / theight)
    };
    // Top Right
    vertices[2] = {
        fwidth,
        0,
        0,
        (x0+fwidth) / twidth,
        1.0f-(y0 / theight)
    };
    // Top Left
    vertices[3] = {
        0,
        0,
        0,
        x0 / twidth,
        1.0f-(y0 / theight)
    };
}

void Anchor::TopRight(CE::Vertex2D* vertices, uint16_t texture_width, uint16_t texture_height,
                    uint16_t width, uint16_t height, uint16_t x0, uint16_t y0
) {
    float twidth = static_cast<float>(texture_width);
    float theight = static_cast<float>(texture_height);
    float fwidth = static_cast<float>(width);
    float fheight = static_cast<float>(height);
    // Bottom Left
    vertices[0] = {
        -fwidth,
        -fheight,
        0,
        x0 / twidth,
        1.0f-((y0+fheight) / theight)
    };
    // Bottom Right
    vertices[1] = {
        0,
        -fheight,
        0,
        (x0+fwidth) / twidth,
        1.0f-((y0+fheight) / theight)
    };
    // Top Right
    vertices[2] = {
        0,
        0,
        0,
        (x0+fwidth) / twidth,
        1.0f-(y0 / theight)
    };
    // Top Left
    vertices[3] = {
        -fwidth,
        0,
        0,
        x0 / twidth,
        1.0f-(y0 / theight)
    };
}

void Anchor::BottomLeft(CE::Vertex2D* vertices, uint16_t texture_width, uint16_t texture_height,
                    uint16_t width, uint16_t height, uint16_t x0, uint16_t y0
) {
    float twidth = static_cast<float>(texture_width);
    float theight = static_cast<float>(texture_height);
    float fwidth = static_cast<float>(width);
    float fheight = static_cast<float>(height);
    // Bottom Left
    vertices[0] = {
        0,
        0,
        0,
        x0 / twidth,
        1.0f-((y0+fheight) / theight)
    };
    // Bottom Right
    vertices[1] = {
        fwidth,
        0,
        0,
        (x0+fwidth) / twidth,
        1.0f-((y0+fheight) / theight)
    };
    // Top Right
    vertices[2] = {
        fwidth,
        fheight,
        0,
        (x0+fwidth) / twidth,
        1.0f-(y0 / theight)
    };
    // Top Left
    vertices[3] = {
        0,
        fheight,
        0,
        x0 / twidth,
        1.0f-(y0 / theight)
    };
}

void Anchor::BottomRight(CE::Vertex2D* vertices, uint16_t texture_width, uint16_t texture_height,
                    uint16_t width, uint16_t height, uint16_t x0, uint16_t y0
) {
    float twidth = static_cast<float>(texture_width);
    float theight = static_cast<float>(texture_height);
    float fwidth = static_cast<float>(width);
    float fheight = static_cast<float>(height);
    // Bottom Left
    vertices[0] = {
        -fwidth,
        0,
        0,
        x0 / twidth,
        1.0f-((y0+fheight) / theight)
    };
    // Bottom Right
    vertices[1] = {
        0,
        0,
        0,
        (x0+fwidth) / twidth,
        1.0f-((y0+fheight) / theight)
    };
    // Top Right
    vertices[2] = {
        0,
        fheight,
        0,
        (x0+fwidth) / twidth,
        1.0f-(y0 / theight)
    };
    // Top Left
    vertices[3] = {
        -fwidth,
        fheight,
        0,
        x0 / twidth,
        1.0f-(y0 / theight)
    };
}

void Anchor::Center(float* vertices, uint16_t texture_width, uint16_t texture_height,
                    uint16_t width, uint16_t height, uint16_t x0, uint16_t y0
) {
    float sx = static_cast<float>(x0);
    float sy = static_cast<float>(y0);
    float twidth = static_cast<float>(texture_width);
    float theight = static_cast<float>(texture_height);
    float fwidth = static_cast<float>(width);
    float fheight = static_cast<float>(height);
    CalcUVs(vertices, twidth, theight, fwidth, fheight, sx, sy);
    ///Bottom Left
    vertices[0] = 0-(fwidth / 2.0f);                  // X
    vertices[1] = 0-(fheight / 2.0f);                 // Y

    ///Bottom Right
    vertices[5] = 0+(fwidth / 2.0f);
    vertices[6] = 0-(fheight / 2.0f);

    ///Top Right
    vertices[10] = 0+(fwidth / 2.0f);
    vertices[11] = 0+(fheight / 2.0f);

    ///Top Left
    vertices[15] = 0-(fwidth / 2.0f);
    vertices[16] = 0+(fheight / 2.0f);
}

void Anchor::TopLeft(float* vertices, uint16_t texture_width, uint16_t texture_height,
                     uint16_t width, uint16_t height, uint16_t x0, uint16_t y0
) {
    float sx = static_cast<float>(x0);
    float sy = static_cast<float>(y0);
    float twidth = static_cast<float>(texture_width);
    float theight = static_cast<float>(texture_height);
    float fwidth = static_cast<float>(width);
    float fheight = static_cast<float>(height);
    CalcUVs(vertices, twidth, theight, fwidth, fheight, sx, sy);
    //Bottom Left
    vertices[0] = 0.f;
    vertices[1] = -fheight;

    //Bottom Right
    vertices[5] = fwidth;
    vertices[6] = -fheight;

    //Top Right
    vertices[10] = fwidth;
    vertices[11] = 0.f;

    //Top Left
    vertices[15] = 0.f;
    vertices[16] = 0.f;
}

void Anchor::TopRight(float* vertices, uint16_t texture_width, uint16_t texture_height,
                      uint16_t width, uint16_t height, uint16_t x0, uint16_t y0
) {
    float sx = static_cast<float>(x0);
    float sy = static_cast<float>(y0);
    float twidth = static_cast<float>(texture_width);
    float theight = static_cast<float>(texture_height);
    float fwidth = static_cast<float>(width);
    float fheight = static_cast<float>(height);
    CalcUVs(vertices, twidth, theight, fwidth, fheight, sx, sy);
    //Bottom Left
    vertices[0] = -fwidth;
    vertices[1] = -fheight;

    //Bottom Right
    vertices[5] = 0.f;
    vertices[6] = -fheight;

    //Top Right
    vertices[10] = 0.f;
    vertices[11] = 0.f;

    //Top Left
    vertices[15] = -fwidth;
    vertices[16] = 0.f;
}

void Anchor::BottomLeft(float* vertices, uint16_t texture_width, uint16_t texture_height,
                        uint16_t width, uint16_t height, uint16_t x0, uint16_t y0
) {
    float sx = static_cast<float>(x0);
    float sy = static_cast<float>(y0);
    float twidth = static_cast<float>(texture_width);
    float theight = static_cast<float>(texture_height);
    float fwidth = static_cast<float>(width);
    float fheight = static_cast<float>(height);
    CalcUVs(vertices, twidth, theight, fwidth, fheight, sx, sy);
    //Bottom Left
    vertices[0] = 0.0f;
    vertices[1] = 0.0f;

    //Bottom Right
    vertices[5] = fwidth;
    vertices[6] = 0.0f;

    //Top Right
    vertices[10] = fwidth;
    vertices[11] = fheight;

    //Top Left
    vertices[15] = 0.0f;
    vertices[16] = fheight;
}

void Anchor::BottomRight(float* vertices, uint16_t texture_width, uint16_t texture_height,
                         uint16_t width, uint16_t height, uint16_t x0, uint16_t y0
) {
    float sx = static_cast<float>(x0);
    float sy = static_cast<float>(y0);
    float twidth = static_cast<float>(texture_width);
    float theight = static_cast<float>(texture_height);
    float fwidth = static_cast<float>(width);
    float fheight = static_cast<float>(height);
    CalcUVs(vertices, twidth, theight, fwidth, fheight, sx, sy);
    //Bottom Left
    vertices[0] = -fwidth;
    vertices[1] = 0.f;

    //Bottom Right
    vertices[5] = 0.f;
    vertices[6] = 0.f;

    //Top Right
    vertices[10] = 0.f;
    vertices[11] = fheight;

    //Top Left
    vertices[15] = -fwidth;
    vertices[16] = fheight;
}
