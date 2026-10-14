#pragma once

#include "context.h"

namespace CE::Platform::GLFW {
    class Window;
}

namespace CE::Rendering::OpenGL {
    /** Adapts a display-owned GLFW window to the OpenGL presentation contract.
     * DisplaySystem owns GLFW and the window; this adapter borrows the window.
     */
    class GLFWContext final : public Context {
    public:
        explicit GLFWContext(::CE::Platform::GLFW::Window& window, int swap_interval = 1);

        void make_current() override;
        void release_current() override;
        [[nodiscard]] bool is_current() const override;
        [[nodiscard]] ProcAddress proc_address(const char* name) const override;
        void present() override;

    private:
        ::CE::Platform::GLFW::Window& window_;
        int swap_interval_;
    };
}
