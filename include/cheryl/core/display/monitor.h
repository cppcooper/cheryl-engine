#pragma once
#include "viewport.h"
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include "GLFW/glfw3.h"

class GLFWmonitor;

namespace CE {
    // A snapshot of a monitor and its current video mode when the display is created.
    struct Monitor : ViewPort<int> {
        GLFWmonitor* const glfw_monitor;
        Monitor(GLFWmonitor* glfw_mon, int width, int height);
        Monitor(GLFWmonitor* glfw_mon, const GLFWvidmode* mode);
    };
}
