#pragma once

#include <core/display/framebuffer-size.h>
#include <glm.hpp>

namespace CE::RenderAPIs {
    /** Renders using a compatible graphics context. Does not own the display,
     * presentation surface, resource provider, or mutable game state.
     * TODO: Add the top-level frame operation after the published render-state representation
     * and asset draw-submission contract are defined. The operations below are only primitives.
     */
    class iRenderer {
    public:
        virtual ~iRenderer() = default;

        virtual void initialize() = 0;
        virtual void deinitialize() = 0;
        virtual void clear() = 0;
        virtual void set_viewport(FramebufferSize size) = 0;
        virtual void set_depth_test(bool enabled) = 0;
        virtual void set_clear_colour(float r, float g, float b, float a) = 0;
        // A view supplied for a render pass; camera dimensionality does not set depth policy.
        virtual void set_camera_matrices(const glm::mat4& projection, const glm::mat4& view) = 0;
    };
}

namespace CE {
    namespace R = RenderAPIs;
}
