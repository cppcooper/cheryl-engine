#pragma once

#include <core/rendering/renderer.h>
#include <functional>
#include <memory>
#include "context.h"
#include "resource-lifetime.h"

namespace CE::RenderAPIs {
    namespace RendererDetail {
        struct RendererAccess;
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
        bool initialized_ = false;
        bool stopped_ = false;

    public:
        explicit OpenGLRenderer(iOpenGLContext& context);
        ~OpenGLRenderer() override;

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
