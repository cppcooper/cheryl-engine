#pragma once
#include "viewport.h"
#include "GLFW/glfw3.h"

class GLFWmonitor;

namespace CE {
// todo: convert to class? control state modification
//  mode, window size?
    struct Monitor : ViewPort<uint16_t> {
        GLFWmonitor* const glfw_monitor;
        Monitor(GLFWmonitor* glfw_mon, uint16_t width, uint16_t height);
        Monitor(GLFWmonitor* glfw_mon, const GLFWvidmode* mode);
    };
}
