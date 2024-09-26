#include <engine/view/monitor.h>

namespace CE {
    Monitor::Monitor(GLFWmonitor* glfw_mon, const uint16_t width, const uint16_t height)
            : ViewPort(width, height), glfw_monitor(glfw_mon) { }
    Monitor::Monitor(GLFWmonitor* glfw_mon, const GLFWvidmode* mode)
            : ViewPort(mode->width, mode->height), glfw_monitor(glfw_mon) { }
}
