#pragma once

#include "context.h"

namespace CE {
    class Window;
}

namespace CE::RenderAPIs {
    /** Adapts a display-owned GLFW window to the OpenGL presentation contract.
     * DisplaySystem owns GLFW and the window; this adapter borrows the window.
     */
    class GlfwOpenGLContext final : public iOpenGLContext {
    public:
        explicit GlfwOpenGLContext(Window& window, int swap_interval = 1);

        void make_current() override;
        void release_current() override;
        [[nodiscard]] ProcAddress proc_address(const char* name) const override;
        void present() override;

    private:
        Window& window_;
        int swap_interval_;
    };
}
