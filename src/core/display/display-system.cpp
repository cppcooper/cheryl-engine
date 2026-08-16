#include <core/display/display-system.h>
#include <GLFW/glfw3.h>

namespace CE {
    Monitor CreatePrimary() {
        auto p = glfwGetPrimaryMonitor();
        auto vm = glfwGetVideoMode(p);
        return {p, vm};
    }

    DisplaySystem::DisplaySystem() : glfw_monitors(glfwGetMonitors(&num_monitors)), primary_monitor(CreatePrimary()) {
        monitors.push_back(primary_monitor);
        // ReSharper disable once CppDFAConstantConditions
        for (int i = 0; i < num_monitors; ++i) {
            // ReSharper disable once CppDFAUnreachableCode
            if (auto monitor = glfw_monitors[i]; monitor != primary_monitor.glfw_monitor) {
                monitors.emplace_back(monitor, glfwGetVideoMode(monitor));
            }
        }
    }

    Window* DisplaySystem::create_window(Monitor monitor, Enum::window_mode mode, uint16_t width, uint16_t height) {
        windows.emplace_back(monitor, mode, width, height);
        return &windows[windows.size()-1];
    }

    Window* DisplaySystem::create_window(Monitor monitor, Enum::window_mode mode, Resolution res) {
        return create_window(monitor, mode, res.width, res.height);
    }

    Window* DisplaySystem::create_window(Monitor monitor, Enum::window_mode mode) {
        return create_window(monitor, mode, monitor.width, monitor.height);
    }
}
