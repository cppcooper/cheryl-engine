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
