#pragma once

#include "framebuffer-size.h"
#include "viewport.h"
#include <core/subsystems/event-channel.h>

#include <enums.h>

namespace CE {
    /** Display-owned window accessed on the platform owner. Logical dimensions use
     * window coordinates; framebuffer dimensions use drawable pixels and may be zero.
     * Queries do not synchronize with another thread. Copy observed values into the
     * runtime handoff instead of reading a live window from simulation/render workers.
     */
    class iWindow {
    public:
        virtual ~iWindow() = default;
        [[nodiscard]] virtual ViewPort<int> logical_size() const = 0;
        [[nodiscard]] virtual FramebufferSize framebuffer_size() const = 0;
        [[nodiscard]] virtual Enum::window_mode mode() const = 0;
        [[nodiscard]] virtual bool should_close() const = 0;
        /** Consume/rethrow a deferred native callback failure on the platform owner.
         * Pumping adapters check before publishing a successful poll. Backends with
         * no throwing native callbacks retain the default no-op implementation.
         */
        virtual void check_native_failure() const {}
        /** Request positive logical dimensions; query the resulting logical/pixel sizes.
         * Backends define observation freshness and failure behavior after mutation.
         */
        virtual void resize(int width, int height) = 0;
        virtual void set_mode(Enum::window_mode mode) = 0;
        /** Change cursor visibility; pointer capture and relative motion are separate capabilities. */
        virtual void hide_cursor(bool hide) const = 0;
    };

    /** Payload of window_resized_event and legacy "window-resized": copied pixels and a display-borrowed
     * window pointer. Queued delivery does not extend that window's lifetime or permit
     * access from another owner; retain the size alone for deferred consumers.
     */
    struct WindowResized {
        iWindow* window;
        FramebufferSize size;
    };

    inline const SubSystems::EventChannel<WindowResized> window_resized_event{"window-resized"};
}
