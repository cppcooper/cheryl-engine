#pragma once
#include "monitor.h"
#include "window.h"

class GLFWmonitor;

namespace CE {
    struct Resolution {
        uint32_t width : 16{};
        uint32_t height : 16{};
    };
    struct DisplaySystem {
    private:
        int num_monitors{};
    public:
        std::vector<Monitor> monitors;
        std::vector<Window> windows;
        const int& monitor_count = num_monitors;
        GLFWmonitor** const glfw_monitors;
        Monitor primary_monitor;
        Window* active = nullptr;

        explicit DisplaySystem();
        Window* create_window(Monitor monitor, Enum::window_mode mode, uint16_t width, uint16_t height);
        Window* create_window(Monitor monitor, Enum::window_mode mode, Resolution res);
        Window* create_window(Monitor monitor, Enum::window_mode mode);
    };
}
