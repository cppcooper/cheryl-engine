#pragma once

#include <backends/opengl/glfw-backend.h>
#include <core/engine/startup.h>

#include <string>

namespace CE::Engine {
    // Borrowed input retains the context factory's lifetime requirements.
    [[nodiscard]] Startup make_glfw_opengl_startup(
        Input::iInputSystem& input,
        std::string description = "Cheryl application",
        GlfwOpenGLConfig configuration = {},
        RuntimeConfiguration runtime = {}
    );

#if CHERYL_NATIVE_INPUT
    [[nodiscard]] Startup make_glfw_opengl_startup(
        std::string description = "Cheryl application",
        GlfwOpenGLConfig configuration = {},
        RuntimeConfiguration runtime = {}
    );
#endif
}
