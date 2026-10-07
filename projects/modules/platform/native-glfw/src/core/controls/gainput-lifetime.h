#pragma once

#include <internals/exceptions.h>

#include <atomic>

namespace CE::Input {
    // Gainput HID collection has one process-wide owner. Keep this lease through
    // device destruction, including while window callbacks are detached.
    class GainputLifetime final {
        std::atomic_flag& ownership_;
        void* window_;

    public:
        GainputLifetime(std::atomic_flag& ownership, void* window)
        : ownership_(ownership), window_(window) {
            if (ownership_.test_and_set(std::memory_order_acquire))
                throw Exceptions::failed_operation(CE_HERE, "Gainput is already owned by another native input adapter");
        }

        ~GainputLifetime() { ownership_.clear(std::memory_order_release); }

        GainputLifetime(const GainputLifetime&) = delete;
        GainputLifetime& operator=(const GainputLifetime&) = delete;

        void require_window(void* window) const {
            if (window_ != window)
                throw Exceptions::failed_operation(CE_HERE, "Gainput notifications require the original Windows window");
        }
    };
}
