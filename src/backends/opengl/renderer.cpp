#include <backends/opengl/gl.h>
#include <backends/opengl/renderer.h>
#include <backends/opengl/pipeline.h>

#include <internals/exceptions.h>


#include <thread>

namespace CE::RenderAPIs {
    OpenGLRenderer::OpenGLRenderer(iOpenGLContext& context)
    : context_(context) {}

    OpenGLRenderer::~OpenGLRenderer() {
        if (initialized_) {
            try {
                deinitialize();
            } catch (...) {
                // Retained assets must never query a context after its owner is destroyed.
                resources_->abandon();
            }
        }
    }

    void OpenGLRenderer::initialize() {
        if (initialized_) {
            resources_->require_owner();
            context_.make_current();
            resources_->require_current();
            return;
        }
        if (stopped_)
            throw Exceptions::failed_operation(CE_HERE, "Create a new renderer after OpenGL shutdown");
        try {
            context_.make_current();
            if (!context_.is_current())
                throw Exceptions::failed_operation(CE_HERE, "OpenGL initialization requires its current context");
            // The renderer receives procedure addresses through the context interface,
            // without depending on the display's native window type.
            const auto version = gladLoadGLUserPtr(
                [](
                void* user,
                const char* name
            ) ->
                GLADapiproc {
                    return static_cast<iOpenGLContext*>(user)->proc_address(name);
                },
                &context_);
            if (version == 0 || GLAD_VERSION_MAJOR(version) < 3 || (GLAD_VERSION_MAJOR(version) == 3 && GLAD_VERSION_MINOR(version) < 3)) {
                throw Exceptions::failed_operation(CE_HERE, "An OpenGL 3.3 context is required");
            }
            resources_ = std::make_shared<OpenGLResourceLifetime>(std::this_thread::get_id(),
                [&context = context_] { return context.is_current(); });
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        } catch (...) {
            // Preserve the initialization failure even if releasing the context also fails.
            try {
                context_.release_current();
            } catch (...) {}
            throw;
        }
        initialized_ = true;
    }

    void OpenGLRenderer::deinitialize() {
        if (!initialized_)
            return;
        resources_->require_owner();
        context_.make_current();
        resources_->shutdown();
        initialized_ = false;
        stopped_ = true;
        context_.release_current();
    }

    std::shared_ptr<OpenGLResourceLifetime> OpenGLRenderer::resources() const {
        if (!initialized_)
            throw Exceptions::failed_operation(CE_HERE, "OpenGL renderer must be initialized before uploading resources");
        resources_->require_current();
        return resources_;
    }

    void OpenGLRenderer::render(const RenderFrame& frame) {
        const auto domain = resources();
        for (const auto& pass : frame.passes()) {
            for (const auto& packet : pass.draws) {
                validate_draw_packet(packet, pass.constraints);
                const auto* pipeline = dynamic_cast<const Assets::GLSLPipeline*>(packet.material->definition().pipeline.get());
                if (!pipeline || pipeline->resource_domain() != domain.get())
                    throw Exceptions::invalid_args(CE_HERE, "OpenGL frame requires a pipeline from this renderer's resource domain");
                pipeline->draw(*packet.geometry, packet.first_vertex, packet.vertex_count, packet.parameters, pass.constraints);
            }
        }
    }

    void OpenGLRenderer::clear() {
        (void)resources();
        // Depth clears obey the write mask left by the last pipeline draw.
        glDepthMask(GL_TRUE);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRenderer::maintain_resources() { resources()->collect(); }

    void OpenGLRenderer::set_viewport(const FramebufferSize size) {
        (void)resources();
        if (size.width < 0 || size.height < 0)
            throw Exceptions::invalid_args(CE_HERE, "Framebuffer dimensions cannot be negative");
        glViewport(0, 0, size.width, size.height);
    }

    void OpenGLRenderer::set_depth_test(const bool enabled) {
        (void)resources();
        if (enabled)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);
    }

    void OpenGLRenderer::set_clear_colour(const float r, const float g, const float b, const float a) {
        (void)resources();
        glClearColor(r, g, b, a);
    }

    void OpenGLRenderer::set_camera_matrices(const glm::mat4& projection, const glm::mat4& view) {
        (void)resources();
        projection_ = projection;
        view_ = view;
    }
}
