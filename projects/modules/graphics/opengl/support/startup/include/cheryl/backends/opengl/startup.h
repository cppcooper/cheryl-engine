#pragma once

#include <backends/opengl/glfw-backend.h>
#include <core/engine/startup.h>

#include <string>

namespace CE::Engine {
    /** Configure command-line startup for GLFW/OpenGL with borrowed input.
     * The adapter must survive context teardown. initialize() runs on the platform
     * owner; this factory registers options but does not create the context.
     * @param input Input adapter borrowed by the eventual context.
     * @param description Application description for command-line help.
     * @param configuration Initial window, presentation and execution defaults.
     * @param runtime Initial simulation and polling defaults.
     * @return A single-use startup object; command-line options override its defaults.
     */
    [[nodiscard]] Startup make_glfw_opengl_startup(
        Input::iInputSystem& input,
        std::string description = "Cheryl application",
        GlfwOpenGLConfig configuration = {},
        RuntimeConfiguration runtime = {}
    );

#if CHERYL_NATIVE_INPUT
    /** Configure GLFW/OpenGL startup with an owned native input adapter.
     * Available when CHERYL_NATIVE_INPUT is enabled. initialize() creates the
     * context on the platform owner; validation and construction follow Startup.
     * @param description Application description for command-line help.
     * @param configuration Initial window, presentation and execution defaults.
     * @param runtime Initial simulation and polling defaults.
     * @return A single-use startup object owning its backend option state.
     */
    [[nodiscard]] Startup make_glfw_opengl_startup(
        std::string description = "Cheryl application",
        GlfwOpenGLConfig configuration = {},
        RuntimeConfiguration runtime = {}
    );
#endif
}
