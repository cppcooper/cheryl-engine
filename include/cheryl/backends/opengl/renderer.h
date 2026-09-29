#pragma once

#include "context.h"
#include "resource-lifetime.h"
#include <core/rendering/renderer.h>
#include <memory>

namespace CE::RenderAPIs {
    /** Implements rendering commands using a separately owned OpenGL context.
     * initialize() makes that context current before loading GL entry points;
     * deinitialize() deletes all tracked GPU resources before releasing it.
     * A stopped renderer cannot be restarted with its old cached assets.
     */
    class OpenGLRenderer final : public iRenderer {
    public:
        explicit OpenGLRenderer(iOpenGLContext& context);
        ~OpenGLRenderer() override;

        void initialize() override;
        void deinitialize() override;
        void render(const RenderFrame& frame) override;
        void clear() override;
        void set_viewport(FramebufferSize size) override;
        void set_depth_test(bool enabled) override;
        void set_clear_colour(float r, float g, float b, float a) override;
        void set_camera_matrices(const glm::mat4& projection, const glm::mat4& view) override;
        [[nodiscard]] std::shared_ptr<OpenGLResourceLifetime> resources() const;

    private:
        iOpenGLContext& context_;
        std::shared_ptr<OpenGLResourceLifetime> resources_;
        bool initialized_ = false;
        bool stopped_ = false;
    };
}
