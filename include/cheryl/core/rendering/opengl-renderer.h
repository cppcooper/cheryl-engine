#pragma once

#include "opengl-context.h"
#include "renderer.h"

namespace CE::RenderAPIs {
    /** Implements rendering commands using a separately owned OpenGL context.
     * initialize() makes that context current before loading GL entry points;
     * deinitialize() releases it after renderer-owned resources are torn down.
     */
    class OpenGLRenderer final : public iRenderer {
    public:
        explicit OpenGLRenderer(iOpenGLContext& context);

        void initialize() override;
        void deinitialize() override;
        void render(const RenderFrame& frame) override;
        void clear() override;
        void set_viewport(FramebufferSize size) override;
        void set_depth_test(bool enabled) override;
        void set_clear_colour(float r, float g, float b, float a) override;
        void set_camera_matrices(const glm::mat4& projection, const glm::mat4& view) override;

    private:
        iOpenGLContext& context_;
    };
}
