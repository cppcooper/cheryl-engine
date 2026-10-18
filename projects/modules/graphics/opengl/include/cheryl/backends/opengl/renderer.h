#pragma once

#include <cheryl/export.h>

#include <core/rendering/renderer.h>
#include <functional>
#include <memory>
#include "context.h"
#include "resource-lifetime.h"

namespace CE::Rendering::OpenGL {
    namespace Detail {
        struct RendererAccess;
        class DebugOutput;
    }
    /** Implements rendering commands using a separately owned OpenGL context.
     * initialize() makes that context current before loading GL entry points;
     * deinitialize() deletes all tracked GPU resources before releasing it.
     * The supplied context must outlive this renderer. Destruction attempts owner-thread
     * cleanup; failed context recovery invalidates retained handles without OpenGL calls.
     * A stopped renderer cannot be restarted with its old cached assets.
     */
    class CE_TYPE_API Renderer final : public ::CE::Rendering::Renderer {
        ::CE::Rendering::OpenGL::Context& context_;
        std::function<void(::CE::Rendering::OpenGL::Context&)> native_loader_;
        std::shared_ptr<::CE::Rendering::OpenGL::ResourceLifetime> resources_;
        glm::mat4 projection_{1.0f};
        glm::mat4 view_{1.0f};
        ::CE::Platform::FramebufferSize viewport_{};
        bool initialized_ = false;
        bool stopped_ = false;
        bool destroying_ = false;
        bool native_diagnostics_ = false;
        std::unique_ptr<::CE::Rendering::OpenGL::Detail::DebugOutput> debug_output_;
        int native_major_ = 0;
        int native_minor_ = 0;

    public:
        CE_OPENGL_API explicit Renderer(::CE::Rendering::OpenGL::Context& context);
        CE_OPENGL_API ~Renderer() override;
        // Opt in before initialization. Observe renderer-owned commands only.
        // GL 4.3/KHR_debug is optional; existing host callbacks remain owned by host.
        CE_OPENGL_API void set_native_diagnostics(bool enabled);

        CE_OPENGL_API void initialize() override;
        CE_OPENGL_API void deinitialize() override;
        CE_OPENGL_API void maintain_resources() override;
        CE_OPENGL_API void render(const ::CE::Rendering::Frame& frame) override;
        CE_OPENGL_API void clear() override;
        CE_OPENGL_API void set_viewport(::CE::Platform::FramebufferSize size) override;
        CE_OPENGL_API void set_depth_test(bool enabled) override;
        CE_OPENGL_API void set_clear_colour(float r, float g, float b, float a) override;
        CE_OPENGL_API void set_camera_matrices(const glm::mat4& projection, const glm::mat4& view) override;
        [[nodiscard]] CE_OPENGL_API std::shared_ptr<::CE::Rendering::OpenGL::ResourceLifetime> resources() const;

    private:
        friend struct ::CE::Rendering::OpenGL::Detail::RendererAccess;
    };
}

/**
 * @fn CE::Rendering::OpenGL::Renderer::Renderer(Context& context)
 * @brief Borrow a compatible OpenGL presentation context without initializing it.
 * @param context Adapter/window kept alive through renderer destruction and resource cleanup.
 * Initialize on its graphics owner before resource uploads or draws; construction performs no native calls.
 */

/**
 * @fn CE::Rendering::OpenGL::Renderer::~Renderer()
 * @brief Attempt owner-thread native cleanup while the borrowed context is still alive.
 * If cleanup/context recovery fails, report it and abandon the lifetime without unsafe native calls. Prefer explicit deinitialize() after quiescing loading/rendering; destruction is not cross-thread cancellation.
 */

/**
 * @fn void CE::Rendering::OpenGL::Renderer::set_native_diagnostics(bool enabled)
 * @brief Opt into renderer-command OpenGL debug observation before initialization.
 * @param enabled Requested observation; GL 4.3/KHR_debug support is optional.
 * @throws CE::Exceptions::failed_operation Once initialized or stopped.
 * Existing host callbacks remain host-owned; unsupported diagnostics do not reject an otherwise supported renderer.
 */

/**
 * @fn void CE::Rendering::OpenGL::Renderer::initialize()
 * @brief Make the borrowed context current, load entry points and establish an OpenGL 3.3-or-newer resource domain.
 * Call on the intended context owner. Already-initialized calls require that owner and reselect the context; a stopped renderer throws failed_operation and cannot restart. Startup failure attempts context release without masking the original error; publication of the domain occurs only after native startup succeeds.
 */

/**
 * @fn void CE::Rendering::OpenGL::Renderer::deinitialize()
 * @brief Unbind the current program, delete all tracked resources, stop diagnostics and release the context.
 * No-op before initialization/after completed shutdown. Otherwise requires the original owner, makes its context current and permanently stops this renderer. Retained logical handles become unusable; quiesce loading/frame consumption first. Native/context failures may propagate, so keep the borrowed context alive for recovery.
 */

/**
 * @fn void CE::Rendering::OpenGL::Renderer::maintain_resources()
 * @brief Unbind the current program and collect retired native objects even without a fresh frame.
 * Requires initialized live owner/current context; does not make another context current. Optional diagnostic reporting occurs on the owner. It neither recycles simulation frames nor presents.
 */

/**
 * @fn void CE::Rendering::OpenGL::Renderer::render(const CE::Rendering::Frame& frame)
 * @brief Play active passes and packets in authored order on the live owner/current context.
 * @param frame Stable published frame borrowed through the call; retain it without writes/recycle.
 * Every packet validates common contracts and must use a same-domain OpenGL pipeline/geometry/images/samplers. Copied packet values drive playback; live cameras/assets are not queried. Failures may leave earlier draws/state applied. Does not clear, present or recycle the frame.
 */

/**
 * @fn void CE::Rendering::OpenGL::Renderer::clear()
 * @brief Clear color and depth buffers after enabling depth writes and disabling scissor.
 * Requires initialized live owner/current context; current clear color applies. Does not present or restore the previous depth-write/scissor state.
 */

/**
 * @fn void CE::Rendering::OpenGL::Renderer::set_viewport(CE::Platform::FramebufferSize size)
 * @brief Set native viewport origin to zero and retain the playback framebuffer extent.
 * @param size Nonnegative pixel width/height; zero minimized extents are allowed.
 * Negative dimensions throw invalid_args before viewport mutation; live owner/current context is required. Retained clips resolve against this stored pixel extent.
 */

/**
 * @fn void CE::Rendering::OpenGL::Renderer::set_depth_test(bool enabled)
 * @brief Set the legacy native depth-test enable flag on the live owner/current context.
 * @param enabled Whether to enable GL_DEPTH_TEST.
 * Retained pipeline draws later reapply their complete fixed depth state; camera dimensionality does not determine it.
 */

/**
 * @fn void CE::Rendering::OpenGL::Renderer::set_clear_colour(float r, float g, float b, float a)
 * @brief Set native clear color on the live owner/current context.
 * @param r Red component passed to OpenGL.
 * @param g Green component passed to OpenGL.
 * @param b Blue component passed to OpenGL.
 * @param a Alpha component passed to OpenGL.
 * No additional CPU range/finiteness validation or immediate clear occurs.
 */

/**
 * @fn void CE::Rendering::OpenGL::Renderer::set_camera_matrices(const glm::mat4& projection, const glm::mat4& view)
 * @brief Copy legacy renderer matrix observations on the live owner/current context.
 * @param projection Borrowed matrix copied during the call.
 * @param view Borrowed matrix copied during the call.
 * Retained packet playback uses its resolved parameter values rather than consulting a live camera or these legacy observations.
 */

/**
 * @fn std::shared_ptr<CE::Rendering::OpenGL::ResourceLifetime> CE::Rendering::OpenGL::Renderer::resources() const
 * @return Retained native lifetime after checking initialized state and live owner/current context.
 * @throws CE::Exceptions::failed_operation Before initialization or for foreign/stopped/noncurrent native access. Retaining the returned lifetime does not keep the borrowed platform context alive.
 * @see [OpenGL backend](cheryl-guide:modules/opengl)
 */
