#pragma once

#include <core/engine/engine-context.h>
#include <enums.h>

#include <memory>
#include <string>

namespace CE::Input {
    class iInputSystem;
}

namespace CE::Engine {
    struct GlfwOpenGLConfig {
        int width = 1280;
        int height = 720;
        Enum::window_mode mode = Enum::window_mode::NORMAL;
        std::string title; // Empty uses the original randomized window titles.
        int swap_interval = 1;
        ExecutionOptions execution;
    };

    /** Assemble the GLFW display/window, its OpenGL presentation context,
     * the OpenGL renderer and resource provider, and the selected input adapter.
     * The default overload owns a dedicated input adapter. The explicit-input
     * overload borrows that adapter; the caller keeps it alive through context teardown.
     * Runtime initialization makes the
     * context current and attaches input before game initialization.
     */
    [[nodiscard]] std::unique_ptr<EngineContext> make_glfw_opengl_context(Input::iInputSystem& input, const GlfwOpenGLConfig& config = {});
#ifndef CHERYL_SANDBOX_BUILD
    [[nodiscard]] std::unique_ptr<EngineContext> make_glfw_opengl_context(const GlfwOpenGLConfig& config = {});
#endif
}
