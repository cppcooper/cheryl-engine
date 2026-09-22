#include <core/display/monitor.h>

namespace CE {
    Monitor::Monitor(GLFWmonitor* glfw_mon, const int width, const int height) :
        ViewPort(width, height), glfw_monitor(glfw_mon) {
    }
    Monitor::Monitor(GLFWmonitor* glfw_mon, const GLFWvidmode* mode) :
        ViewPort(mode->width, mode->height), glfw_monitor(glfw_mon) {
    }
}
