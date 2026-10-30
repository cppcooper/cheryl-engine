#pragma once

#include <cheryl/export.h>

#include <core/display/framebuffer-size.h>
#include <core/rendering/render-frame.h>
#include <core/rendering/graphics-domain.h>
#include <core/subsystems/event-bus.h>
#include <glm.hpp>

namespace CE::Rendering {
    /** Renders using a compatible graphics context. Does not own the display,
     * presentation surface, resource provider, or mutable game state.
     * render() consumes ordered passes from a published frame on the graphics thread.
     * The operations below are backend primitives for the frame renderer.
     * Borrowed frame/matrix inputs must remain stable through each call. The caller
     * brackets native use with backend initialization/teardown and retains its context.
     * A backend failure may leave native state changed or part of a frame drawn.
     */
    class CE_API Renderer {
        CE::Events::EventBus::Publisher event_publisher_;
        CE::Events::EventBus::Delivery terminal_delivery_;
        std::uint64_t published_sequence_ = 0;
        bool publishing_domain_ = false;

    public:
        virtual ~Renderer() = default;

        virtual void initialize() = 0;
        virtual void deinitialize() = 0;
        /** Optional portable domain token; custom adapters may return an empty token. */
        [[nodiscard]] virtual GraphicsDomain graphics_domain() const noexcept { return {}; }
        /** Select routing on the graphics owner before initialization. Runtime supplies
         * its Context bus. Optional terminal_delivery must queue without invoking
         * inline, as EventBus::Delivery requires, and retain its targets through
         * execution/cancellation. Destructors only submit owned terminal records;
         * without this transport, inspect the retained token after destruction.
         */
        void publish_events_to(
            CE::Events::EventBus::Publisher publisher,
            CE::Events::EventBus::Delivery terminal_delivery = CE::Events::EventBus::Delivery{}
        ) noexcept;
        // Platform/context-owner maintenance, even without a new frame. Deferred
        // resource backends collect final-owner retirements here; others may do nothing.
        virtual void maintain_resources() = 0;
        virtual void render(const ::CE::Rendering::Frame& frame) = 0;
        virtual void clear() = 0;
        virtual void set_viewport(::CE::Platform::FramebufferSize size) = 0;
        virtual void set_depth_test(bool enabled) = 0;
        virtual void set_clear_colour(float r, float g, float b, float a) = 0;
        // A view supplied for a render pass; camera dimensionality does not set depth policy.
        virtual void set_camera_matrices(const glm::mat4& projection, const glm::mat4& view) = 0;

    protected:
        // Backend operation-final publication. Never call from a native-resource
        // lock; destructor paths defer rather than entering listeners.
        void publish_graphics_domain_changes(bool destroying = false) noexcept;
    };
}

/**
 * @fn CE::Rendering::Renderer::~Renderer()
 * @brief Destroy the renderer adapter; backend-specific teardown requirements still apply to borrowed contexts.
 */

/**
 * @fn void CE::Rendering::Renderer::initialize()
 * @brief Initialize native rendering on the graphics/context owner before uploads or draws.
 * Concrete backends define capability validation, repeat-call behavior and failure cleanup; this interface does not promise restart after deinitialize().
 */

/**
 * @fn void CE::Rendering::Renderer::deinitialize()
 * @brief Stop native rendering and retire backend resources before the borrowed context/window is destroyed.
 * Call on the graphics/context owner after frame consumption and loading are quiescent. Concrete backend cleanup errors may propagate.
 */

/**
 * @fn void CE::Rendering::Renderer::maintain_resources()
 * @brief Perform owner-thread maintenance even when no new frame is available.
 * Deferred-release backends collect retired resources here; other adapters may do nothing. Requires initialized live backend state as defined by the adapter.
 */

/**
 * @fn void CE::Rendering::Renderer::render(const CE::Rendering::Frame& frame)
 * @brief Consume active passes and draw packets in authored order without accessing mutable game state.
 * @param frame Published frame borrowed through the call; exclude writes/recycle until completion.
 * Requires graphics owner/current context and retained compatible resource domains. A backend failure may leave partial rendering/native state changes; no rollback or presentation is implied.
 */

/**
 * @fn void CE::Rendering::Renderer::clear()
 * @brief Clear the current rendering target on the graphics owner under backend-specific color/depth rules. Does not present or publish a frame.
 */

/**
 * @fn void CE::Rendering::Renderer::set_viewport(CE::Platform::FramebufferSize size)
 * @brief Set backend viewport extent in framebuffer pixels.
 * @param size Pixel dimensions; validity/minimized-window behavior belongs to the concrete backend.
 * Requires its native graphics owner; does not mutate a camera or frame.
 */

/**
 * @fn void CE::Rendering::Renderer::set_depth_test(bool enabled)
 * @brief Select a legacy backend depth-test primitive.
 * @param enabled Requested comparison enable state.
 * Retained pipeline draws apply their own fixed state; camera dimensionality does not choose this policy.
 */

/**
 * @fn void CE::Rendering::Renderer::set_clear_colour(float r, float g, float b, float a)
 * @brief Set the backend clear color on its graphics owner.
 * @param r Red component.
 * @param g Green component.
 * @param b Blue component.
 * @param a Alpha component.
 * Range/conversion behavior belongs to the backend; this operation does not issue a clear or alter draw-material alpha.
 */

/**
 * @fn void CE::Rendering::Renderer::set_camera_matrices(const glm::mat4& projection, const glm::mat4& view)
 * @brief Supply copied legacy backend pass matrices without retaining a camera.
 * @param projection Stable projection borrowed through the call.
 * @param view Stable view borrowed through the call.
 * Does not select depth policy or publish immutable pass storage.
 * @see [Rendering guide](cheryl-guide:rendering/pipelines-and-materials)
 */
