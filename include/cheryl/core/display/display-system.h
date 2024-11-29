#pragma once
#include "monitor.h"
#include "window.h"

class GLFWmonitor;

/* DisplaySystem
 * This pod exposes the monitors and windows vectors to the public
 * This was done as a temporary solution to not knowing how this will be used
 *
 * The display system is likely to be used by either the camera or renderer or both.
 * The camera needs actual data from this system, it should be associated with a window
 * and needs the dimensions of that window.
 *
 * todo: revise implementation to remove direct dependency on OpenGL
 * todo: revise access modifiers
 * todo: determine usage/interactions
 */

namespace CE {
    struct Resolution {
        uint32_t width : 16{};
        uint32_t height : 16{};
    };
    struct DisplaySystem {
        std::vector<Monitor> monitors;
        std::vector<Window> windows;
        const int& monitor_count = num_monitors;
        GLFWmonitor** const glfw_monitors;
        Monitor primary_monitor;
        Window* active = nullptr;
    private:
        int num_monitors{};
    public:
        explicit DisplaySystem();
        Window* create_window(Monitor monitor, Enum::window_mode mode, uint16_t width, uint16_t height);
        Window* create_window(Monitor monitor, Enum::window_mode mode, Resolution res);
        Window* create_window(Monitor monitor, Enum::window_mode mode);
    };
}
