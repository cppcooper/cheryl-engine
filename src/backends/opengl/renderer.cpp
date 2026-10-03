#include <backends/opengl/renderer.h>
#include "renderer-internal.h"
#include "upload-check.h"
#include <backends/opengl/gl.h>
#include <backends/opengl/pipeline.h>

#include <internals/exceptions.h>
#include <internals/failure-reporting.h>
#include <internals/compile-time-logging.hpp>

#include <thread>
#include <utility>

namespace CE::RenderAPIs {
    namespace {
        void load_native_functions(iOpenGLContext& context) {
            struct Loader {
                iOpenGLContext& context;
                std::exception_ptr failure;
            } loader{context};
            const auto version = gladLoadGLUserPtr(
                [](void* user, const char* name) noexcept -> GLADapiproc {
                    auto& loader = *static_cast<Loader*>(user);
                    if (loader.failure)
                        return nullptr;
                    try {
                        return loader.context.proc_address(name);
                    } catch (...) {
                        loader.failure = std::current_exception();
                        return nullptr;
                    }
                },
                &loader
            );
            if (loader.failure)
                std::rethrow_exception(loader.failure);
            if (version == 0 || GLAD_VERSION_MAJOR(version) < 3 || (GLAD_VERSION_MAJOR(version) == 3 && GLAD_VERSION_MINOR(version) < 3))
                throw Exceptions::failed_operation(CE_HERE, "An OpenGL 3.3 context is required");
        }
    }

    void RendererDetail::RendererAccess::set_native_loader(OpenGLRenderer& renderer, std::function<void(iOpenGLContext&)> loader) {
        if (!loader)
            throw Exceptions::invalid_args(CE_HERE, "Native loader must not be empty");
        if (renderer.initialized_ || renderer.stopped_ || renderer.resources_)
            throw Exceptions::failed_operation(CE_HERE, "Native loader must be configured before renderer startup");
        renderer.native_loader_ = std::move(loader);
    }

    OpenGLRenderer::OpenGLRenderer(iOpenGLContext& context)
    : context_(context) {}

    OpenGLRenderer::~OpenGLRenderer() {
        destroying_ = true;
        if (initialized_) {
            try {
                deinitialize();
            } catch (...) {
                // Retained assets must never query a context after its owner is destroyed.
                Diagnostics::report_failure("OpenGL renderer destruction", std::current_exception());
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
            if (native_loader_)
                native_loader_(context_);
            else
                load_native_functions(context_);
            require_no_gl_error("OpenGL error before renderer startup configuration");
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            require_no_gl_error("OpenGL renderer startup configuration failed");
            if (!context_.is_current())
                throw Exceptions::failed_operation(CE_HERE, "OpenGL context was lost during renderer startup");
            // Publish the domain only after loading and default state succeed.
            resources_ = std::make_shared<OpenGLResourceLifetime>(std::this_thread::get_id(), [&context = context_] {
                return context.is_current();
            });
        } catch (...) {
            // Preserve the initialization failure even if releasing the context also fails.
            try {
                context_.release_current();
            } catch (...) {
                Diagnostics::report_failure("OpenGL initialization context release", std::current_exception());
            }
            throw;
        }
        initialized_ = true;
        CE_LOG_INFO(CE::enginelog, "subsystem=renderer domain={} operation=initialize outcome=ready backend=opengl baseline=3.3",
                    resources_->diagnostics().domain);
    }

    void OpenGLRenderer::deinitialize() {
        if (!initialized_)
            return;
        resources_->require_owner();
        context_.make_current();
        resources_->require_current();
        // Deleting a current program only flags it until it is unbound. Finish
        // native deletion even when the borrowed window/context stays alive.
        glUseProgram(0);
        resources_->shutdown();
        initialized_ = false;
        stopped_ = true;
        context_.release_current();
        if (!destroying_) {
            CE_LOG_INFO(CE::enginelog, "subsystem=renderer domain={} operation=shutdown outcome=completed", resources_->diagnostics().domain);
            CE_LOG_DEBUG(CE::enginelog, "subsystem=native_resources domain={} operation=shutdown tracked={} deleted={} abandoned={}",
                         resources_->diagnostics().domain, resources_->diagnostics().tracked,
                         resources_->diagnostics().deleted, resources_->diagnostics().abandoned);
        }
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

    void OpenGLRenderer::maintain_resources() {
        const auto domain = resources();
        // The final frame may retire the program still bound by its last draw.
        // A later draw selects its own program; idle collection must free this one.
        glUseProgram(0);
        domain->collect();
    }

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
