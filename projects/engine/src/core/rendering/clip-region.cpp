#include <core/rendering/clip-region.h>

#include <internals/exceptions.h>

#include <algorithm>
#include <cmath>

namespace CE::RenderAPIs {
    namespace {
        void validate_rectangle(const ClipRect2D& rectangle) {
            if (!std::isfinite(rectangle.left) || !std::isfinite(rectangle.top) || !std::isfinite(rectangle.right) ||
                !std::isfinite(rectangle.bottom) || rectangle.left > rectangle.right || rectangle.top > rectangle.bottom)
                throw Exceptions::invalid_args(CE_HERE, "Clip edges must be finite and ordered");
        }
    }

    void validate_clip_region(const ClipRegion2D& region) {
        validate_rectangle(region.rectangle);
        if (!std::isfinite(region.logical_width) || !std::isfinite(region.logical_height) || region.logical_width <= 0 ||
            region.logical_height <= 0)
            throw Exceptions::invalid_args(CE_HERE, "Clip logical dimensions must be finite and positive");
    }

    ClipRect2D intersect_clip_rects(const ClipRect2D& first, const ClipRect2D& second) {
        validate_rectangle(first);
        validate_rectangle(second);
        const auto left = std::max(first.left, second.left);
        const auto top = std::max(first.top, second.top);
        return {left, top, std::max(left, std::min(first.right, second.right)), std::max(top, std::min(first.bottom, second.bottom))};
    }

    PixelClipRect2D resolve_clip_region(const ClipRegion2D& region, const FramebufferSize framebuffer) {
        validate_clip_region(region);
        if (framebuffer.width < 0 || framebuffer.height < 0)
            throw Exceptions::invalid_args(CE_HERE, "Framebuffer dimensions cannot be negative");
        const auto& rectangle = region.rectangle;
        const auto left = std::clamp(rectangle.left, 0.0, region.logical_width);
        const auto right = std::clamp(rectangle.right, 0.0, region.logical_width);
        const auto top = std::clamp(rectangle.top, 0.0, region.logical_height);
        const auto bottom = std::clamp(rectangle.bottom, 0.0, region.logical_height);
        if (left == right || top == bottom || framebuffer.width == 0 || framebuffer.height == 0)
            return {};
        // Divide only clamped coordinates: even extreme finite inputs cannot
        // overflow the conversion into the framebuffer's integer range.
        return {static_cast<int>(std::floor(left / region.logical_width * framebuffer.width)),
            static_cast<int>(std::floor(top / region.logical_height * framebuffer.height)),
            static_cast<int>(std::ceil(right / region.logical_width * framebuffer.width)),
            static_cast<int>(std::ceil(bottom / region.logical_height * framebuffer.height))};
    }
}
