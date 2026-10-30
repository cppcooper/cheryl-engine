#include "gl-internal.h"
#include <backends/opengl/glfw-backend.h>

#include <backends/opengl/glfw-context.h>
#include <backends/opengl/renderer.h>
#include <backends/opengl/resource-provider.h>
#include <core/controls/input-system.h>
#include <core/display/display-system.h>
#include <internals/exceptions.h>

#include <memory>
#include <utility>

namespace CE::Application::GLFWOpenGL {
    template <typename InputAdapter>
    static std::unique_ptr<CE::Runtime::Context> assemble_context(InputAdapter&& input, const Options& config) {
        if (config.width <= 0 || config.height <= 0 || config.swap_interval < 0)
            throw Exceptions::invalid_args(CE_HERE, "Window dimensions and swap interval must be valid");

        // Constructing the GLFW display initializes GLFW; its windows are destroyed before
        // the display releases that library lifetime, even if a later step throws.
        auto display = std::make_unique<CE::Platform::GLFW::DisplaySystem>();
        auto* window = display->create_opengl_window(display->primary_monitor(), config.mode, config.width, config.height, config.title);
        display->activate_window(*window);

        auto surface = std::make_unique<CE::Rendering::OpenGL::GLFWContext>(*window, config.swap_interval);
        auto renderer = std::make_unique<CE::Rendering::OpenGL::Renderer>(*surface);
        auto resources = std::make_unique<CE::Rendering::OpenGL::ResourceProvider>(*renderer);
        return std::make_unique<CE::Runtime::Context>(
            std::move(display), std::move(surface), std::move(renderer), std::move(resources), std::forward<InputAdapter>(input),
            config.execution
        );
    }

    std::unique_ptr<CE::Runtime::Context> make_context(CE::Input::System& input, const Options& config) {
        return assemble_context(input, config);
    }

#if CHERYL_NATIVE_INPUT
    std::unique_ptr<CE::Runtime::Context> make_context(const Options& config) {
        return assemble_context(std::make_unique<CE::Platform::GLFW::InputSystem>(), config);
    }
#endif
}
