#pragma once

#include <core/engine/engine-context.h>
#include <enums.h>

#include <memory>
#include <string>

namespace CE::Input {
    class System;
}

namespace CE::Application::GLFWOpenGL {
    struct Options {
        int width = 1280;
        int height = 720;
        CE::Platform::WindowMode mode = CE::Platform::WindowMode::NORMAL;
        std::string title; // Empty uses the original randomized window titles.
        int swap_interval = 1;
        CE::Runtime::ExecutionOptions execution;
    };

    /** Assemble the GLFW display/window, its OpenGL presentation context,
     * the OpenGL renderer and resource provider, and the selected input adapter.
     * The default overload owns a dedicated input adapter. The explicit-input
     * overload borrows that adapter; the caller keeps it alive through context teardown.
     * Runtime initialization makes the
     * context current and attaches input before game initialization.
     */
    [[nodiscard]] std::unique_ptr<CE::Runtime::Context> make_context(CE::Input::System& input, const Options& config = Options{});
#if CHERYL_NATIVE_INPUT
    [[nodiscard]] std::unique_ptr<CE::Runtime::Context> make_context(const Options& config = Options{});
#endif
}
