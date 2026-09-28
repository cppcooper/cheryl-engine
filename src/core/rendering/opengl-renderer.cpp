#include <core/rendering/opengl-renderer.h>

#include <internals/exceptions.h>

namespace CE::RenderAPIs {
    namespace {
        [[noreturn]] void renderer_pending() {
            throw Exceptions::failed_operation(CE_HERE, "OpenGL renderer is a skeleton");
        }
    }

    OpenGLRenderer::OpenGLRenderer(iOpenGLContext& context) : context_(context) {}

    // TODO: Restore GL entry-point loading and render state after the context becomes current.
    void OpenGLRenderer::initialize() { renderer_pending(); }
    void OpenGLRenderer::deinitialize() { renderer_pending(); }
    void OpenGLRenderer::render(const RenderFrame&) { renderer_pending(); }
    void OpenGLRenderer::clear() { renderer_pending(); }
    void OpenGLRenderer::set_viewport(FramebufferSize) { renderer_pending(); }
    void OpenGLRenderer::set_depth_test(bool) { renderer_pending(); }
    void OpenGLRenderer::set_clear_colour(float, float, float, float) { renderer_pending(); }
    void OpenGLRenderer::set_camera_matrices(const glm::mat4&, const glm::mat4&) { renderer_pending(); }
}
