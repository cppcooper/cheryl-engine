#pragma once

#include <core/display/framebuffer-size.h>
#include <core/rendering/render-frame.h>
#include <glm.hpp>

namespace CE::RenderAPIs {
    /** Renders using a compatible graphics context. Does not own the display,
     * presentation surface, resource provider, or mutable game state.
     * render() consumes ordered passes from a published frame on the graphics thread.
     * The operations below are backend primitives for the frame renderer.
     * Borrowed frame/matrix inputs must remain stable through each call. The caller
     * brackets native use with backend initialization/teardown and retains its context.
     * A backend failure may leave native state changed or part of a frame drawn.
     */
    class iRenderer {
    public:
        virtual ~iRenderer() = default;

        virtual void initialize() = 0;
        virtual void deinitialize() = 0;
        // Platform/context-owner maintenance, even without a new frame. Deferred
        // resource backends collect final-owner retirements here; others may do nothing.
        virtual void maintain_resources() = 0;
        virtual void render(const RenderFrame& frame) = 0;
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
