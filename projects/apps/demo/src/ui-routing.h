#pragma once

#include <core/display/framebuffer-size.h>
#include <core/display/viewport.h>

#include <utility>

namespace DemoUiRouting {
    inline bool drawable(const CE::Platform::Dimensions<int> logical, const CE::Platform::FramebufferSize framebuffer) noexcept {
        return logical.width > 0 && logical.height > 0 && framebuffer.width > 0 && framebuffer.height > 0;
    }

    // Native listeners stage effects only during a selected, uncancelled input batch.
    class Effects final {
        bool selected_ = false;
        bool cancelled_ = false;
        bool reset_ = false;
        bool image_ = false;

    public:
        void begin(bool enabled) noexcept {
            cancelled_ = false;
            selected_ = enabled;
            if (!enabled)
                cancel();
        }
        void select(bool selected) noexcept { selected_ = selected && !cancelled_; }
        void cancel() noexcept {
            cancelled_ = true;
            selected_ = reset_ = image_ = false;
        }
        void request_reset() noexcept {
            if (selected_)
                reset_ = true;
        }
        void request_image() noexcept {
            if (selected_)
                image_ = !image_;
        }
        bool finish_image() noexcept {
            selected_ = false;
            return std::exchange(image_, false);
        }
        bool consume_reset() noexcept { return std::exchange(reset_, false); }
    };
}
