#pragma once

#include <core/rendering/renderer.h>
#include <functional>
#include <memory>
#include "context.h"
#include "resource-lifetime.h"

namespace CE::Rendering::OpenGL {
    namespace Detail {
        struct RendererAccess;
        class DebugOutput;
    }
    /** Implements rendering commands using a separately owned OpenGL context.
     * initialize() makes that context current before loading GL entry points;
     * deinitialize() deletes all tracked GPU resources before releasing it.
     * The supplied context must outlive this renderer. Destruction attempts owner-thread
     * cleanup; failed context recovery invalidates retained handles without OpenGL calls.
     * A stopped renderer cannot be restarted with its old cached assets.
     */
    class Renderer final : public ::CE::Rendering::Renderer {
        ::CE::Rendering::OpenGL::Context& context_;
        std::function<void(::CE::Rendering::OpenGL::Context&)> native_loader_;
        std::shared_ptr<::CE::Rendering::OpenGL::ResourceLifetime> resources_;
        glm::mat4 projection_{1.0f};
        glm::mat4 view_{1.0f};
        ::CE::Platform::FramebufferSize viewport_{};
        bool initialized_ = false;
        bool stopped_ = false;
        bool destroying_ = false;
        bool native_diagnostics_ = false;
        std::unique_ptr<::CE::Rendering::OpenGL::Detail::DebugOutput> debug_output_;
        int native_major_ = 0;
        int native_minor_ = 0;

    public:
        explicit Renderer(::CE::Rendering::OpenGL::Context& context);
        ~Renderer() override;
        // Opt in before initialization. Observe renderer-owned commands only.
        // GL 4.3/KHR_debug is optional; existing host callbacks remain owned by host.
        void set_native_diagnostics(bool enabled);

        void initialize() override;
        void deinitialize() override;
        void maintain_resources() override;
        void render(const ::CE::Rendering::Frame& frame) override;
        void clear() override;
        void set_viewport(::CE::Platform::FramebufferSize size) override;
        void set_depth_test(bool enabled) override;
        void set_clear_colour(float r, float g, float b, float a) override;
        void set_camera_matrices(const glm::mat4& projection, const glm::mat4& view) override;
        [[nodiscard]] std::shared_ptr<::CE::Rendering::OpenGL::ResourceLifetime> resources() const;

    private:
        friend struct ::CE::Rendering::OpenGL::Detail::RendererAccess;
    };
}
