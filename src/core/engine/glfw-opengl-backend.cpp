#include <core/engine/glfw-opengl-backend.h>

#include <internals/exceptions.h>

namespace CE::Engine {
    std::unique_ptr<EngineContext> make_glfw_opengl_context(Input::iInputSystem&) {
        // TODO: Initialize GLFW and choose OpenGL window hints before constructing DisplaySystem.
        // Create its window, adapt it as GlfwOpenGLContext, construct OpenGLRenderer and
        // OpenGLResourceProvider, then transfer ownership into EngineContext in that order.
        // Move shader compilation into the resource provider when restoring the GL implementation.
        throw Exceptions::failed_operation(CE_HERE, "GLFW/OpenGL composition is a skeleton");
    }
}
