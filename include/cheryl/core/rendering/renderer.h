#pragma once

#include <core/display/display-system-interface.h>
#include <glm.hpp>

#include <memory>

namespace CE::Assets {
    struct ResourceProvider;
}

namespace CE::RenderAPIs {
    struct iRenderer {
        // TODO: Revisit display ownership. Graphics and window backends are intended to vary
        // independently, but the renderer currently owns the display system. Engine composition
        // may be the cleaner ownership/injection boundary if those choices remain independent.
        // TODO: Specify renderer thread affinity. OpenGL contexts are current to one thread at a time,
        // while GLFW window/event operations have main-thread restrictions. A render thread therefore
        // needs explicit context ownership and command submission rather than arbitrary calls across threads.
        std::unique_ptr<iDisplaySystem> display;
        virtual ~iRenderer() = default;

        virtual void initialize_libraries() = 0;
        virtual void initialize_rendering_context() = 0;
        virtual void deinitialize() = 0;
        virtual void clear() = 0;
        virtual void set_viewport(FramebufferSize size) = 0;
        virtual void set_depth_test(bool enabled) = 0;
        virtual void set_clear_colour(float r, float g, float b, float a) = 0;
        virtual void set_camera_matrices(const glm::mat4& projection, const glm::mat4& view) = 0;
        virtual void swap_buffer() = 0;
        [[nodiscard]] virtual Assets::ResourceProvider& resources() = 0;
    };
}

namespace CE {
    namespace R = RenderAPIs;
}
