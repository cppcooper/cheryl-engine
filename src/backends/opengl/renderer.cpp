#include <backends/opengl/renderer.h>
#include <backends/opengl/gl.h>

#include <internals/exceptions.h>

#include <thread>

namespace CE::RenderAPIs {
    namespace {
        [[noreturn]] void renderer_pending() {
            throw Exceptions::failed_operation(CE_HERE, "OpenGL renderer is a skeleton");
        }
    }

    OpenGLRenderer::OpenGLRenderer(iOpenGLContext& context) : context_(context) {}

    OpenGLRenderer::~OpenGLRenderer() {
        if (initialized_) {
            try { deinitialize(); } catch (...) { /* The context must be shut down on its owner thread. */ }
        }
    }

    void OpenGLRenderer::initialize() {
        if (initialized_) {
            resources_->require_current();
            return;
        }
        if (stopped_)
            throw Exceptions::failed_operation(CE_HERE, "Create a new renderer after OpenGL shutdown");
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
            resources_ = std::make_shared<OpenGLResourceLifetime>(std::this_thread::get_id());
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
        resources_->shutdown();
        context_.release_current();
        initialized_ = false;
        stopped_ = true;
    }
    std::shared_ptr<OpenGLResourceLifetime> OpenGLRenderer::resources() const {
        if (!initialized_)
            throw Exceptions::failed_operation(CE_HERE, "OpenGL renderer must be initialized before uploading resources");
        resources_->require_current();
        return resources_;
    }

    void OpenGLRenderer::render(const RenderFrame&) {
        resources()->collect();
        renderer_pending();
    }
    void OpenGLRenderer::clear() {
        resources()->collect();
        renderer_pending();
    }
    void OpenGLRenderer::set_viewport(FramebufferSize) { renderer_pending(); }
    void OpenGLRenderer::set_depth_test(bool) { renderer_pending(); }
    void OpenGLRenderer::set_clear_colour(float, float, float, float) { renderer_pending(); }
    void OpenGLRenderer::set_camera_matrices(const glm::mat4&, const glm::mat4&) { renderer_pending(); }
}
