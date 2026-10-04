#pragma once

#include "framebuffer-size.h"
#include "viewport.h"

#include <enums.h>

namespace CE {
    // The window operations available to engines and input implementations.
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
        virtual void resize(int width, int height) = 0;
        virtual void set_mode(Enum::window_mode mode) = 0;
        virtual void hide_cursor(bool hide) const = 0;
    };

    // Payload of the "window-resized" event. The window is owned by its display.
    struct WindowResized {
        iWindow* window;
        FramebufferSize size;
    };
}
