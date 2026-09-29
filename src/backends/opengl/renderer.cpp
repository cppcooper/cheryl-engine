#include <backends/opengl/renderer.h>
#include <backends/opengl/gl.h>

#include <internals/exceptions.h>

namespace CE::RenderAPIs {
    namespace {
        [[noreturn]] void renderer_pending() {
            throw Exceptions::failed_operation(CE_HERE, "OpenGL renderer is a skeleton");
        }
    }

    OpenGLRenderer::OpenGLRenderer(iOpenGLContext& context) : context_(context) {}

    void OpenGLRenderer::initialize() {
        if (initialized_)
            return;
        context_.make_current();
        try {
            // The renderer receives procedure addresses through the context interface,
            // without depending on the display's native window type.
            const auto version = gladLoadGLUserPtr(
                [](void* user, const char* name) -> GLADapiproc {
                    return static_cast<iOpenGLContext*>(user)->proc_address(name);
                },
                &context_);
            if (version == 0 || GLAD_VERSION_MAJOR(version) < 3 ||
                (GLAD_VERSION_MAJOR(version) == 3 && GLAD_VERSION_MINOR(version) < 3)) {
                throw Exceptions::failed_operation(CE_HERE, "An OpenGL 3.3 context is required");
            }
        }
        catch (...) {
            context_.release_current();
            throw;
        }
        initialized_ = true;
    }

    void OpenGLRenderer::deinitialize() {
        if (!initialized_)
            return;
        context_.release_current();
        initialized_ = false;
    }
    void OpenGLRenderer::render(const RenderFrame&) { renderer_pending(); }
    void OpenGLRenderer::clear() { renderer_pending(); }
    void OpenGLRenderer::set_viewport(FramebufferSize) { renderer_pending(); }
    void OpenGLRenderer::set_depth_test(bool) { renderer_pending(); }
    void OpenGLRenderer::set_clear_colour(float, float, float, float) { renderer_pending(); }
    void OpenGLRenderer::set_camera_matrices(const glm::mat4&, const glm::mat4&) { renderer_pending(); }
}
