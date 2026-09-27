#include <math/anchor.h>

#include <assets/primitives/vertex.h>
#include <internals/exceptions.h>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace CE::math {
    namespace {
        void validate(const Pivot pivot, const std::uint32_t texture_width, const std::uint32_t texture_height) {
            if (!std::isfinite(pivot.x) || !std::isfinite(pivot.y) || pivot.x < 0.0f || pivot.x > 1.0f ||
                pivot.y < 0.0f || pivot.y > 1.0f) {
                throw Exceptions::invalid_args(CE_HERE, "A pivot must be normalized to the [0, 1] range");
            }
            if (texture_width == 0 || texture_height == 0) {
                throw Exceptions::invalid_args(CE_HERE, "A texture must have non-zero dimensions");
            }
        }

        void set_vertex(float* vertices, const std::size_t index, const Vertex2D& vertex) {
            const auto offset = index * 5;
            vertices[offset] = vertex.x;
            vertices[offset + 1] = vertex.y;
            vertices[offset + 2] = vertex.z;
            vertices[offset + 3] = vertex.u;
            vertices[offset + 4] = vertex.v;
        }

        struct Corners {
            Vertex2D bottom_left;
            Vertex2D bottom_right;
            Vertex2D top_right;
            Vertex2D top_left;
        };

        Corners make_corners(const Pivot pivot, const std::uint32_t texture_width,
                             const std::uint32_t texture_height, const std::uint32_t width,
                             const std::uint32_t height, const std::uint32_t x0, const std::uint32_t y0) {
            validate(pivot, texture_width, texture_height);

            const auto tw = static_cast<float>(texture_width);
            const auto th = static_cast<float>(texture_height);
            const auto fw = static_cast<float>(width);
            const auto fh = static_cast<float>(height);
            const auto sx = static_cast<float>(x0);
            const auto sy = static_cast<float>(y0);

            // Shift around the pivot, then flip the source rectangle's top-left
            // pixel coordinates into OpenGL UV space for both quad layouts.
            const float left = -pivot.x * fw;
            const float right = (1.0f - pivot.x) * fw;
            const float bottom = (pivot.y - 1.0f) * fh;
            const float top = pivot.y * fh;
            const float u0 = sx / tw;
            const float u1 = (sx + fw) / tw;
            const float v0 = 1.0f - ((sy + fh) / th);
            const float v1 = 1.0f - (sy / th);

            return {{left, bottom, 0.0f, u0, v0}, {right, bottom, 0.0f, u1, v0},
                    {right, top, 0.0f, u1, v1}, {left, top, 0.0f, u0, v1}};
        }
    }

    AnchorType get_anchor(const std::string& anchor) {
        if (anchor == "TL")
            return AnchorType::TopLeft;
        if (anchor == "TC")
            return AnchorType::TopCenter;
        if (anchor == "TR")
            return AnchorType::TopRight;
        if (anchor == "CL" || anchor == "ML")
            return AnchorType::CenterLeft;
        if (anchor == "CR" || anchor == "MR")
            return AnchorType::CenterRight;
        if (anchor == "BL")
            return AnchorType::BottomLeft;
        if (anchor == "BC")
            return AnchorType::BottomCenter;
        if (anchor == "BR")
            return AnchorType::BottomRight;
        return AnchorType::Center;
    }

    Pivot get_pivot(const AnchorType anchor) {
        switch (anchor) {
        case AnchorType::TopLeft:
            return {0.0f, 0.0f};
        case AnchorType::TopCenter:
            return {0.5f, 0.0f};
        case AnchorType::TopRight:
            return {1.0f, 0.0f};
        case AnchorType::CenterLeft:
            return {0.0f, 0.5f};
        case AnchorType::CenterRight:
            return {1.0f, 0.5f};
        case AnchorType::BottomLeft:
            return {0.0f, 1.0f};
        case AnchorType::BottomCenter:
            return {0.5f, 1.0f};
        case AnchorType::BottomRight:
            return {1.0f, 1.0f};
        case AnchorType::Center:
            return {0.5f, 0.5f};
        }
        return {0.5f, 0.5f};
    }

    Quad Anchor::MakeQuad(const Pivot pivot, const std::uint32_t texture_width, const std::uint32_t texture_height,
                          const std::uint32_t width, const std::uint32_t height, const std::uint32_t x0,
                          const std::uint32_t y0) {
        const auto corners = make_corners(pivot, texture_width, texture_height, width, height, x0, y0);

        Quad quad{};
        quad.vertices[0] = corners.bottom_left;
        quad.vertices[1] = corners.bottom_right;
        quad.vertices[2] = corners.top_right;
        quad.vertices[3] = corners.bottom_left;
        quad.vertices[4] = corners.top_right;
        quad.vertices[5] = corners.top_left;
        return quad;
    }

    QuadTriangleStrip Anchor::MakeQuadStrip(const Pivot pivot, const std::uint32_t texture_width,
                                           const std::uint32_t texture_height, const std::uint32_t width,
                                           const std::uint32_t height, const std::uint32_t x0, const std::uint32_t y0) {
        const auto corners = make_corners(pivot, texture_width, texture_height, width, height, x0, y0);
        Triangle first{};
        first.vertices = {corners.bottom_left, corners.bottom_right, corners.top_left};
        return {first, corners.top_right};
    }

    void Anchor::MakePivot(const Pivot pivot, Vertex2D* vertices, const std::uint32_t texture_width,
                           const std::uint32_t texture_height, const std::uint32_t width, const std::uint32_t height,
                           const std::uint32_t x0, const std::uint32_t y0) {
        const auto quad = MakeQuad(pivot, texture_width, texture_height, width, height, x0, y0);
        std::copy(quad.vertices.begin(), quad.vertices.end(), vertices);
    }

    void Anchor::MakePivot(const Pivot pivot, float* vertices, const std::uint32_t texture_width,
                           const std::uint32_t texture_height, const std::uint32_t width, const std::uint32_t height,
                           const std::uint32_t x0, const std::uint32_t y0) {
        // The interleaved float API presents the same quad as the typed API.
        const auto quad = MakeQuad(pivot, texture_width, texture_height, width, height, x0, y0);
        for (std::size_t i = 0; i < quad.vertices.size(); ++i) set_vertex(vertices, i, quad.vertices[i]);
    }

    void Anchor::MakeAnchor(const AnchorType type, Vertex2D* vertices, const std::uint32_t texture_width,
                            const std::uint32_t texture_height, const std::uint32_t width, const std::uint32_t height,
                            const std::uint32_t x0, const std::uint32_t y0) {
        MakePivot(get_pivot(type), vertices, texture_width, texture_height, width, height, x0, y0);
    }

    void Anchor::MakeAnchor(const AnchorType type, float* vertices, const std::uint32_t texture_width,
                            const std::uint32_t texture_height, const std::uint32_t width, const std::uint32_t height,
                            const std::uint32_t x0, const std::uint32_t y0) {
        MakePivot(get_pivot(type), vertices, texture_width, texture_height, width, height, x0, y0);
    }

#define CE_ANCHOR_WRAPPER(name, value)                                                                                 \
    void Anchor::name(Vertex2D* vertices, const std::uint32_t texture_width, const std::uint32_t texture_height,       \
                      const std::uint32_t width, const std::uint32_t height, const std::uint32_t x0,                   \
                      const std::uint32_t y0) {                                                                        \
        MakePivot(value, vertices, texture_width, texture_height, width, height, x0, y0);                              \
    }                                                                                                                  \
    void Anchor::name(float* vertices, const std::uint32_t texture_width, const std::uint32_t texture_height,          \
                      const std::uint32_t width, const std::uint32_t height, const std::uint32_t x0,                   \
                      const std::uint32_t y0) {                                                                        \
        MakePivot(value, vertices, texture_width, texture_height, width, height, x0, y0);                              \
    }

    CE_ANCHOR_WRAPPER(Center, get_pivot(AnchorType::Center))
    CE_ANCHOR_WRAPPER(TopLeft, get_pivot(AnchorType::TopLeft))
    CE_ANCHOR_WRAPPER(TopRight, get_pivot(AnchorType::TopRight))
    CE_ANCHOR_WRAPPER(BottomLeft, get_pivot(AnchorType::BottomLeft))
    CE_ANCHOR_WRAPPER(BottomRight, get_pivot(AnchorType::BottomRight))

#undef CE_ANCHOR_WRAPPER
}
