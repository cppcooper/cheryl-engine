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
