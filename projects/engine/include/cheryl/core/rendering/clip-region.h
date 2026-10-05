#pragma once

#include <core/display/framebuffer-size.h>

namespace CE::RenderAPIs {
    // Axis-aligned edges in logical coordinates, with the origin at the top left.
    // Equal edges describe an empty rectangle; inverted/non-finite edges are invalid.
    struct ClipRect2D {
        double left = 0;
        double top = 0;
        double right = 0;
        double bottom = 0;
        bool operator==(const ClipRect2D&) const = default;
    };

    // Copy the logical extent with the rectangle into the retained draw. Playback
    // maps it to the current framebuffer without consulting a live UI or window.
    struct ClipRegion2D {
        ClipRect2D rectangle;
        double logical_width = 0;
        double logical_height = 0;
        bool operator==(const ClipRegion2D&) const = default;
    };

    // Top-left framebuffer edges; the right/bottom edges are excluded.
    struct PixelClipRect2D {
        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;

        [[nodiscard]] bool empty() const { return left == right || top == bottom; }
        bool operator==(const PixelClipRect2D&) const = default;
    };

    void validate_clip_region(const ClipRegion2D& region);
    [[nodiscard]] ClipRect2D intersect_clip_rects(const ClipRect2D& first, const ClipRect2D& second);
    // Clamp to the logical viewport, then round nonempty edges outwards: floor
    // left/top, ceil right/bottom. Empty clips and zero-sized framebuffers draw nothing.
    [[nodiscard]] PixelClipRect2D resolve_clip_region(const ClipRegion2D& region, FramebufferSize framebuffer);
}
