#include <backends/opengl/glfw-backend.h>

#include <backends/opengl/glfw-context.h>
#include <backends/opengl/renderer.h>
#include <backends/opengl/resource-provider.h>
#include <core/controls/input-system.h>
#include <core/display/display-system.h>
#include <internals/exceptions.h>

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <memory>
#include <utility>

namespace CE::Engine {
    template <typename InputAdapter>
    static std::unique_ptr<EngineContext> assemble_context(InputAdapter&& input, const GlfwOpenGLConfig& config) {
        if (config.width <= 0 || config.height <= 0 || config.swap_interval < 0)
            throw Exceptions::invalid_args(CE_HERE, "Window dimensions and swap interval must be valid");

        // Constructing the GLFW display initializes GLFW; its windows are destroyed before
        // the display releases that library lifetime, even if a later step throws.
        auto display = std::make_unique<DisplaySystem>();
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
        auto* window = display->create_window(display->primary_monitor(), config.mode, config.width, config.height, config.title);
        display->activate_window(*window);

        auto surface = std::make_unique<RenderAPIs::GlfwOpenGLContext>(*window, config.swap_interval);
        auto renderer = std::make_unique<RenderAPIs::OpenGLRenderer>(*surface);
        auto resources = std::make_unique<Assets::OpenGLResourceProvider>(*renderer);
        return std::make_unique<EngineContext>(
            std::move(display), std::move(surface), std::move(renderer), std::move(resources), std::forward<InputAdapter>(input),
            config.execution
        );
    }

    std::unique_ptr<EngineContext> make_glfw_opengl_context(Input::iInputSystem& input, const GlfwOpenGLConfig& config) {
        return assemble_context(input, config);
    }

#ifndef CHERYL_SANDBOX_BUILD
    std::unique_ptr<EngineContext> make_glfw_opengl_context(const GlfwOpenGLConfig& config) {
        return assemble_context(std::make_unique<Input::InputSystem>(), config);
    }
#endif
}
