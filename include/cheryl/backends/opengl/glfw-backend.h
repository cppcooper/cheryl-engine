#pragma once

#include <core/engine/engine-context.h>

#include <memory>

namespace CE::Input {
    class iInputSystem;
}

namespace CE::Engine {
    /** Assemble the GLFW display/window, its OpenGL presentation context,
     * the OpenGL renderer and resource provider, and the selected input adapter.
     * The result owns everything except input. No glEngine subclass is needed.
     */
    [[nodiscard]] std::unique_ptr<EngineContext> make_glfw_opengl_context(Input::iInputSystem& input);
}
