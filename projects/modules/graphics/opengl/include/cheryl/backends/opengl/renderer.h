#pragma once

#include <core/rendering/renderer.h>
#include <functional>
#include <memory>
#include "context.h"
#include "resource-lifetime.h"

namespace CE::RenderAPIs {
    namespace RendererDetail {
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
    class OpenGLRenderer final : public iRenderer {
        iOpenGLContext& context_;
        std::function<void(iOpenGLContext&)> native_loader_;
        std::shared_ptr<OpenGLResourceLifetime> resources_;
        glm::mat4 projection_{1.0f};
        glm::mat4 view_{1.0f};
        FramebufferSize viewport_{};
        bool initialized_ = false;
        bool stopped_ = false;
        bool destroying_ = false;
        bool native_diagnostics_ = false;
        std::unique_ptr<RendererDetail::DebugOutput> debug_output_;
        int native_major_ = 0;
        int native_minor_ = 0;

    public:
        explicit OpenGLRenderer(iOpenGLContext& context);
        ~OpenGLRenderer() override;
        // Opt in before initialization. Observe renderer-owned commands only.
        // GL 4.3/KHR_debug is optional; existing host callbacks remain owned by host.
        void set_native_diagnostics(bool enabled);

        void initialize() override;
        void deinitialize() override;
        void maintain_resources() override;
        void render(const RenderFrame& frame) override;
        void clear() override;
        void set_viewport(FramebufferSize size) override;
        void set_depth_test(bool enabled) override;
        void set_clear_colour(float r, float g, float b, float a) override;
        void set_camera_matrices(const glm::mat4& projection, const glm::mat4& view) override;
        [[nodiscard]] std::shared_ptr<OpenGLResourceLifetime> resources() const;

    private:
        friend struct RendererDetail::RendererAccess;
    };
}
