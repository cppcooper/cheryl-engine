#pragma once

#include <cheryl/export.h>

#include <core/engine/engine-context.h>
#include <enums.h>

#include <memory>
#include <string>

namespace CE::Input {
    class System;
}

namespace CE::Application::GLFWOpenGL {
    /** Window, presentation and runtime defaults copied during context assembly. */
    struct Options {
        int width = 1280; ///< Positive requested window width; framebuffer pixels may differ.
        int height = 720; ///< Positive requested window height.
        CE::Platform::WindowMode mode = CE::Platform::WindowMode::NORMAL; ///< Platform window mode passed to the native display adapter.
        std::string title; ///< Owned title; empty selects the native adapter's randomized title.
        int swap_interval = 1; ///< Nonnegative presentation refresh-interval request.
        CE::Runtime::ExecutionOptions execution; ///< Runtime execution policy copied into the assembled Context.
    };

    /** Assemble the GLFW display/window, its OpenGL presentation context,
     * the OpenGL renderer and resource provider, and the selected input adapter.
     * The default overload owns a dedicated input adapter. The explicit-input
     * overload borrows that adapter; the caller keeps it alive through context teardown.
     * Runtime initialization makes the
     * context current and attaches input before game initialization.
     */
    [[nodiscard]] CE_OPENGL_API std::unique_ptr<CE::Runtime::Context> make_context(CE::Input::System& input, const Options& config = Options{});
#if CHERYL_NATIVE_INPUT
    [[nodiscard]] CE_OPENGL_API std::unique_ptr<CE::Runtime::Context> make_context(const Options& config = Options{});
#endif
}

/**
 * @fn std::unique_ptr<CE::Runtime::Context> CE::Application::GLFWOpenGL::make_context(CE::Input::System& input, const Options& config)
 * @brief Assemble owned display/window, OpenGL presentation, renderer/resources and borrowed input.
 * @param input Adapter retained by the caller through context teardown; construction does not initialize or poll it.
 * @param config Window/presentation/execution defaults copied during assembly.
 * @return Owned runtime Context; initialize it on the platform owner before game initialization.
 * Nonpositive dimensions or negative swap interval throw invalid_args. Display/window/allocation failures clean up completed owned components; no runtime rendering starts here.
 * @see [OpenGL integration](cheryl-guide:modules/opengl)
 */

/**
 * @fn std::unique_ptr<CE::Runtime::Context> CE::Application::GLFWOpenGL::make_context(const Options& config)
 * @brief Assemble the same backend with an owned dedicated native input adapter.
 * @param config Copied window/presentation/execution defaults.
 * @return Owned runtime Context including its input adapter.
 * Available only when CHERYL_NATIVE_INPUT is enabled; otherwise supply explicit input. Validation/ownership/initialization requirements match the explicit-input overload.
 * @see [OpenGL integration](cheryl-guide:modules/opengl)
 */
