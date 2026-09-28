#pragma once

#include "opengl-context.h"

namespace CE {
    class Window;
}

namespace CE::RenderAPIs {
    /** Adapts a display-owned GLFW window to the OpenGL presentation contract.
     * GLFW initialization and window creation belong to platform bootstrap, not here.
     */
    class GlfwOpenGLContext final : public iOpenGLContext {
    public:
        explicit GlfwOpenGLContext(Window& window);

        void make_current() override;
        void release_current() override;
        [[nodiscard]] ProcAddress proc_address(const char* name) const override;
        void present() override;

    private:
        Window& window_;
    };
}
