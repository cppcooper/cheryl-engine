#pragma once
#include "viewport.h"
#include "monitor.h"
#include <enums.h>

class GLFWwindow;

namespace CE {
// todo: convert to class? control state modification
//  mode, window size?
    struct Window final : ViewPort<uint16_t> {
        Enum::window_mode window_mode;
        const Monitor monitor;
        GLFWwindow* const glfw_window;

        explicit Window(const Monitor& monitor, Enum::window_mode mode, uint16_t width, uint16_t height);
        // Activates window - only one window can be active at a time (glfwMakeContextCurrent)
        void activate() const;
        void resize(uint16_t width, uint16_t height);
        void set_mode(Enum::window_mode mode);
        void hide_cursor(bool hide) const;
    private:
        int xpos, ypos;
    };
}
