#include <backends/opengl/glfw-context.h>

#include <core/display/window.h>
#include "../../core/display/glfw-diagnostics.h"
#include <internals/exceptions.h>
#include <internals/compile-time-logging.hpp>

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

namespace CE::RenderAPIs {
    GlfwOpenGLContext::GlfwOpenGLContext(Window& window, const int swap_interval)
    : window_(window), swap_interval_(swap_interval) {
        if (swap_interval < 0)
            throw Exceptions::invalid_args(CE_HERE, "Swap interval must not be negative");
        CE_LOG_DEBUG(
            CE::enginelog, "subsystem=context domain={} operation=selection swap_interval={}", window_.diagnostic_id(), swap_interval_
        );
    }

    void GlfwOpenGLContext::make_current() {
        glfwMakeContextCurrent(window_.native_handle());
        if (!is_current()) {
            DisplayDetail::report_glfw_diagnostics("make_current", true);
            throw Exceptions::failed_operation(CE_HERE, "Could not make the window's OpenGL context current");
        }
        DisplayDetail::report_glfw_diagnostics("make_current");
        glfwSwapInterval(swap_interval_);
    }

    void GlfwOpenGLContext::release_current() {
        if (!is_current())
            throw Exceptions::failed_operation(CE_HERE, "The window's OpenGL context is not current on this thread");
        glfwMakeContextCurrent(nullptr);
    }

    bool GlfwOpenGLContext::is_current() const {
        return glfwGetCurrentContext() == window_.native_handle();
    }

    GlfwOpenGLContext::ProcAddress GlfwOpenGLContext::proc_address(const char* name) const {
        if (!name)
            throw Exceptions::invalid_args(CE_HERE, "OpenGL procedure name must not be null");
        if (!is_current())
            throw Exceptions::failed_operation(CE_HERE, "OpenGL procedure lookup requires the current context");
        return glfwGetProcAddress(name);
    }

    void GlfwOpenGLContext::present() {
        if (!is_current())
            throw Exceptions::failed_operation(CE_HERE, "Present requires the current OpenGL context");
        glfwSwapBuffers(window_.native_handle());
    }
}
