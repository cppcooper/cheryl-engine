#include "gl-internal.h"
#include <backends/opengl/glfw-context.h>

#include <core/display/window.h>
#include <backends/native-glfw/diagnostics.h>
#include <internals/exceptions.h>
#include <internals/compile-time-logging.hpp>

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
        window_.make_context_current(swap_interval_);
    }

    void GLFWContext::release_current() {
        window_.release_context();
    }

    bool GLFWContext::is_current() const {
        return window_.context_current();
    }

    GLFWContext::ProcAddress GLFWContext::proc_address(const char* name) const {
        return window_.context_proc_address(name);
    }

    void GLFWContext::present() {
        window_.swap_buffers();
    }
}
