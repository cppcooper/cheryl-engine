#pragma once

#include <cheryl/export.h>

#include "context.h"

namespace CE::Platform::GLFW {
    class Window;
}

namespace CE::Rendering::OpenGL {
    /** Adapts a display-owned GLFW window to the OpenGL presentation contract.
     * DisplaySystem owns GLFW and the window; this adapter borrows the window.
     */
    class CE_TYPE_API GLFWContext final : public Context {
    public:
        CE_OPENGL_API explicit GLFWContext(::CE::Platform::GLFW::Window& window, int swap_interval = 1);

        CE_OPENGL_API void make_current() override;
        CE_OPENGL_API void release_current() override;
        [[nodiscard]] CE_OPENGL_API bool is_current() const override;
        [[nodiscard]] CE_OPENGL_API ProcAddress proc_address(const char* name) const override;
        CE_OPENGL_API void present() override;

    private:
        ::CE::Platform::GLFW::Window& window_;
        int swap_interval_;
    };
}

/**
 * @fn CE::Rendering::OpenGL::GLFWContext::GLFWContext(CE::Platform::GLFW::Window& window, int swap_interval)
 * @brief Borrow a display-owned window and retain the requested presentation interval.
 * @param window GLFW window kept alive until this adapter and its renderer are destroyed.
 * @param swap_interval Nonnegative refresh-interval request, applied by make_current().
 * @throws CE::Exceptions::invalid_args For a negative interval. Construction does not make the context current.
 */

/**
 * @fn void CE::Rendering::OpenGL::GLFWContext::make_current()
 * @brief Delegate to the borrowed window's owner-thread context selection, applying the stored swap interval.
 */

/**
 * @fn void CE::Rendering::OpenGL::GLFWContext::release_current()
 * @brief Delegate context release to the borrowed window on its permitted owner; no window destruction occurs.
 */

/**
 * @fn bool CE::Rendering::OpenGL::GLFWContext::is_current() const
 * @return The borrowed window's current-context observation for this caller; keep the display/window live.
 */

/**
 * @fn CE::Rendering::OpenGL::Context::ProcAddress CE::Rendering::OpenGL::GLFWContext::proc_address(const char* name) const
 * @brief Resolve a native entry point through the borrowed GLFW window.
 * @param name NUL-terminated name borrowed during the call.
 * @return Native address or null as reported by GLFW; use during owner-thread loader initialization.
 */

/**
 * @fn void CE::Rendering::OpenGL::GLFWContext::present()
 * @brief Swap the borrowed window's buffers on its permitted presentation owner after rendering; no Frame is retained.
 */
