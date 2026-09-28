#include <backends/opengl/glfw-context.h>

#include <internals/exceptions.h>

namespace CE::RenderAPIs {
    GlfwOpenGLContext::GlfwOpenGLContext(Window& window) : window_(window) {}

    // TODO: Make the context created with the display-owned GLFW window current on this thread.
    void GlfwOpenGLContext::make_current() {
        throw Exceptions::failed_operation(CE_HERE, "GLFW OpenGL context is a skeleton");
    }

    void GlfwOpenGLContext::release_current() {
        throw Exceptions::failed_operation(CE_HERE, "GLFW OpenGL context is a skeleton");
    }

    GlfwOpenGLContext::ProcAddress GlfwOpenGLContext::proc_address(const char*) const {
        throw Exceptions::failed_operation(CE_HERE, "GLFW OpenGL context is a skeleton");
    }

    void GlfwOpenGLContext::present() {
        throw Exceptions::failed_operation(CE_HERE, "GLFW OpenGL context is a skeleton");
    }
}
