#pragma once

#include <cstdint>

namespace CE::Rendering::Geometry {
    // Image-pixel values with a top-left origin. Rect coordinates are widened for
    // grid arithmetic; these aggregates perform no bounds/dimension validation.
/** Unsigned top-left-origin image pixel position; aggregate construction performs no bounds validation. */
    struct PixelPoint {
        std::uint32_t x{}; ///< Horizontal pixel offset from the image's left edge.
        std::uint32_t y{}; ///< Vertical pixel offset from the image's top edge.
    };

/** Unsigned image width/height in pixels; consuming operations define whether zero is allowed. */
    struct PixelSize {
        std::uint32_t width{}; ///< Horizontal extent in pixels.
        std::uint32_t height{}; ///< Vertical extent in pixels.
    };

/** Top-left-origin source rectangle with widened coordinates for grid arithmetic; bounds are caller-validated. */
    struct PixelRect {
        std::uint64_t x{}; ///< Widened horizontal source offset in pixels.
        std::uint64_t y{}; ///< Widened vertical source offset in pixels.
        std::uint32_t width{}; ///< Horizontal extent in pixels.
        std::uint32_t height{}; ///< Vertical extent in pixels.
    };
}
