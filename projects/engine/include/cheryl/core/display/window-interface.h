#pragma once

#include <cheryl/export.h>

#include "framebuffer-size.h"
#include "viewport.h"
#include <core/subsystems/event-channel.h>

#include <enums.h>

namespace CE::Platform {
    /** Display-owned window accessed on the platform owner. Logical dimensions use
     * window coordinates; framebuffer dimensions use drawable pixels and may be zero.
     * Queries do not synchronize with another thread. Copy observed values into the
     * runtime handoff instead of reading a live window from simulation/render workers.
     */
    class CE_API Window {
    public:
        virtual ~Window() = default;
        [[nodiscard]] virtual ::CE::Platform::Dimensions<int> logical_size() const = 0;
        [[nodiscard]] virtual ::CE::Platform::FramebufferSize framebuffer_size() const = 0;
        [[nodiscard]] virtual ::CE::Platform::WindowMode mode() const = 0;
        [[nodiscard]] virtual bool should_close() const = 0;
        virtual void check_native_failure() const {}
        virtual void resize(int width, int height) = 0;
        virtual void set_mode(::CE::Platform::WindowMode mode) = 0;
        virtual void hide_cursor(bool hide) const = 0;
    };

    /** Payload of window_resized_event and legacy "window-resized": copied pixels and a display-borrowed
     * window pointer. Queued delivery does not extend that window's lifetime or permit
     * access from another owner; retain the size alone for deferred consumers.
     */
    struct WindowResized {
        ::CE::Platform::Window* window; ///< Borrowed source window; queued payload ownership does not extend its lifetime.
        ::CE::Platform::FramebufferSize size; ///< Copied new drawable dimensions in pixels; safe to retain independently.
    };

    /** Typed pixel-resize channel; shares the name with the separate legacy named channel.
     * @see [Window events](cheryl-guide:runtime/display-and-window-contract)
     */
    inline const ::CE::Events::EventChannel<WindowResized> window_resized_event{"window-resized"};
}

/**
 * @fn CE::Platform::Window::~Window()
 * @brief Release concrete native window state on its required owner.
 * Detach input callbacks and rendering/presentation dependents before destruction.
 * @see [Detailed guide](cheryl-guide:runtime/display-and-window-contract)
 */

/**
 * @fn CE::Platform::Dimensions<int> CE::Platform::Window::logical_size() const
 * @brief Copy logical window-coordinate dimensions on the platform owner.
 * @return Logical size, distinct from framebuffer pixel extent. Observation freshness is backend-defined.
 * @see [Detailed guide](cheryl-guide:runtime/display-and-window-contract)
 */

/**
 * @fn CE::Platform::FramebufferSize CE::Platform::Window::framebuffer_size() const
 * @brief Copy drawable pixel dimensions on the platform owner.
 * @return Framebuffer size; either dimension may be zero while minimized.
 * Copy into runtime handoff state instead of querying the live window from a simulation worker.
 * @see [Detailed guide](cheryl-guide:runtime/display-and-window-contract)
 */

/**
 * @fn CE::Platform::WindowMode CE::Platform::Window::mode() const
 * @brief Read the backend-observed window mode on the platform owner.
 * @see [Detailed guide](cheryl-guide:runtime/display-and-window-contract)
 */

/**
 * @fn bool CE::Platform::Window::should_close() const
 * @brief Observe the backend close-request flag on the platform owner.
 * @return Whether normal runtime shutdown should begin; this query does not destroy the window.
 * @see [Detailed guide](cheryl-guide:runtime/display-and-window-contract)
 */

/**
 * @fn void CE::Platform::Window::check_native_failure() const
 * @brief Consume/rethrow a deferred native callback failure on the platform owner.
 * Pumping adapters check this before publishing a successful poll. The default implementation does nothing for backends without throwing native callbacks.
 * @see [Detailed guide](cheryl-guide:runtime/display-and-window-contract)
 */

/**
 * @fn void CE::Platform::Window::resize(int width, int height)
 * @brief Request a logical size on the platform owner.
 * @param width Positive logical width.
 * @param height Positive logical height.
 * Query resulting logical/pixel sizes afterward; freshness, native constraints and mutation-failure behavior belong to the backend.
 * @see [Detailed guide](cheryl-guide:runtime/display-and-window-contract)
 */

/**
 * @fn void CE::Platform::Window::set_mode(CE::Platform::WindowMode mode)
 * @brief Request a backend-supported window mode on the platform owner.
 * @param mode Requested mode; invalid/unsupported transitions may reject.
 * Does not promise a new render context or update a retained frame automatically.
 * @see [Detailed guide](cheryl-guide:runtime/display-and-window-contract)
 */

/**
 * @fn void CE::Platform::Window::hide_cursor(bool hide) const
 * @brief Set cursor visibility on the platform owner.
 * @param hide True hides the native cursor; false shows it.
 * Visibility does not imply relative pointer capture or locked motion.
 * @see [Detailed guide](cheryl-guide:runtime/display-and-window-contract)
 */
