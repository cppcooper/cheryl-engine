#include <backends/opengl/glfw-context.h>

#include <core/display/window.h>
#include <backends/native-glfw/diagnostics.h>
#include <internals/exceptions.h>
#include <internals/compile-time-logging.hpp>

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

namespace CE::Rendering::OpenGL {
    GLFWContext::GLFWContext(::CE::Platform::GLFW::Window& window, const int swap_interval)
    : window_(window), swap_interval_(swap_interval) {
        if (swap_interval < 0)
            throw ::CE::Exceptions::invalid_args(CE_HERE, "Swap interval must not be negative");
        CE_LOG_DEBUG(
            ::CE::Logging::Categories::platform, "subsystem=context domain={} operation=selection swap_interval={}",
            window_.diagnostic_id(), swap_interval_
        );
    }

    void GLFWContext::make_current() {
        glfwMakeContextCurrent(window_.native_handle());
        if (!is_current()) {
            ::CE::Platform::GLFW::Detail::report_glfw_diagnostics("make_current", true);
            throw ::CE::Exceptions::failed_operation(CE_HERE, "Could not make the window's OpenGL context current");
        }
        ::CE::Platform::GLFW::Detail::report_glfw_diagnostics("make_current");
        glfwSwapInterval(swap_interval_);
    }

    void GLFWContext::release_current() {
        if (!is_current())
            throw ::CE::Exceptions::failed_operation(CE_HERE, "The window's OpenGL context is not current on this thread");
        glfwMakeContextCurrent(nullptr);
    }

    bool GLFWContext::is_current() const {
        return glfwGetCurrentContext() == window_.native_handle();
    }

    GLFWContext::ProcAddress GLFWContext::proc_address(const char* name) const {
        if (!name)
            throw ::CE::Exceptions::invalid_args(CE_HERE, "OpenGL procedure name must not be null");
        if (!is_current())
            throw ::CE::Exceptions::failed_operation(CE_HERE, "OpenGL procedure lookup requires the current context");
        return glfwGetProcAddress(name);
    }

    void GLFWContext::present() {
        if (!is_current())
            throw ::CE::Exceptions::failed_operation(CE_HERE, "Present requires the current OpenGL context");
        glfwSwapBuffers(window_.native_handle());
    }
}
