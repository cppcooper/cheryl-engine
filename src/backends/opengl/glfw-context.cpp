#include <backends/opengl/glfw-context.h>

#include <core/display/window.h>
#include <internals/exceptions.h>

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

namespace CE::RenderAPIs {
    GlfwOpenGLContext::GlfwOpenGLContext(Window& window, const int swap_interval) :
        window_(window), swap_interval_(swap_interval) {
        if (swap_interval < 0)
            throw Exceptions::invalid_args(CE_HERE, "Swap interval must not be negative");
    }

    void GlfwOpenGLContext::make_current() {
        glfwMakeContextCurrent(window_.native_handle());
        if (glfwGetCurrentContext() != window_.native_handle())
            throw Exceptions::failed_operation(CE_HERE, "Could not make the window's OpenGL context current");
        glfwSwapInterval(swap_interval_);
    }

    void GlfwOpenGLContext::release_current() {
        if (glfwGetCurrentContext() != window_.native_handle())
            throw Exceptions::failed_operation(CE_HERE, "The window's OpenGL context is not current on this thread");
        glfwMakeContextCurrent(nullptr);
    }

    GlfwOpenGLContext::ProcAddress GlfwOpenGLContext::proc_address(const char* name) const {
        if (!name)
            throw Exceptions::invalid_args(CE_HERE, "OpenGL procedure name must not be null");
        if (glfwGetCurrentContext() != window_.native_handle())
            throw Exceptions::failed_operation(CE_HERE, "OpenGL procedure lookup requires the current context");
        return glfwGetProcAddress(name);
    }

    void GlfwOpenGLContext::present() {
        if (glfwGetCurrentContext() != window_.native_handle())
            throw Exceptions::failed_operation(CE_HERE, "Present requires the current OpenGL context");
        glfwSwapBuffers(window_.native_handle());
    }
}
