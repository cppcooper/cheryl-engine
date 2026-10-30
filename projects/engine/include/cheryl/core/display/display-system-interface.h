#pragma once

#include <cheryl/export.h>

#include "monitor.h"
#include "window-interface.h"

#include <utility>
#include <vector>

namespace CE::Platform {
    /** Platform-owner display/window factory. Created windows remain display-owned;
     * callers borrow them and detach dependent input/render owners before teardown.
     * Monitor references are borrowed inventory snapshots with backend-defined refresh
     * and invalidation points. Copy values before handing them to another thread.
     */
    class CE_API DisplaySystem {
    public:
        virtual ~DisplaySystem() = default;
        [[nodiscard]] virtual const std::vector<::CE::Platform::Monitor>& monitors() const = 0;
        [[nodiscard]] virtual int monitor_count() const = 0;
        [[nodiscard]] virtual const ::CE::Platform::Monitor& primary_monitor() const = 0;
        [[nodiscard]] virtual ::CE::Platform::Window* active_window() const = 0;
        [[nodiscard]] virtual std::pair<float, float> content_scale(const ::CE::Platform::Monitor& monitor) const = 0;
        virtual ::CE::Platform::Window*
        create_window(const ::CE::Platform::Monitor& monitor, ::CE::Platform::WindowMode mode, int width, int height) = 0;
        virtual void activate_window(::CE::Platform::Window& window) = 0;
    };
}

/**
 * @fn CE::Platform::DisplaySystem::~DisplaySystem()
 * @brief Release the concrete display and its owned windows.
 * Input, renderer and presentation users must detach before display destruction on the platform owner.
 * @see [Detailed guide](cheryl-guide:runtime/display-and-window-contract)
 */

/**
 * @fn const std::vector<CE::Platform::Monitor>& CE::Platform::DisplaySystem::monitors() const
 * @brief Borrow the current backend monitor inventory.
 * @return Borrowed snapshots, invalidated by backend-defined refresh or display destruction.
 * Use on the platform owner and copy values for another thread; this call is not a hotplug subscription.
 * @see [Detailed guide](cheryl-guide:runtime/display-and-window-contract)
 */

/**
 * @fn int CE::Platform::DisplaySystem::monitor_count() const
 * @brief Query the backend inventory count on the platform owner.
 * @return Number of monitor snapshots under the backend refresh policy.
 * @see [Detailed guide](cheryl-guide:runtime/display-and-window-contract)
 */

/**
 * @fn const CE::Platform::Monitor& CE::Platform::DisplaySystem::primary_monitor() const
 * @brief Borrow the primary-monitor snapshot.
 * @return A display-owned value with the same refresh/destruction lifetime as monitors().
 * @see [Detailed guide](cheryl-guide:runtime/display-and-window-contract)
 */

/**
 * @fn CE::Platform::Window* CE::Platform::DisplaySystem::active_window() const
 * @brief Borrow the selected rendering window.
 * @return The current display-owned selection, or nullptr when the backend has no selection.
 * Does not activate a graphics context or marshal to the platform owner.
 * @see [Detailed guide](cheryl-guide:runtime/display-and-window-contract)
 */

/**
 * @fn std::pair<float, float> CE::Platform::DisplaySystem::content_scale(const CE::Platform::Monitor& monitor) const
 * @brief Query monitor X/Y content scale on the platform owner.
 * @param monitor Snapshot recognized by this display inventory.
 * @return Scale factors under the concrete backend policy.
 * This does not establish per-window scale transitions or change notifications; unsupported/foreign inventory values can reject.
 * @see [Detailed guide](cheryl-guide:runtime/display-and-window-contract)
 */

/**
 * @fn CE::Platform::Window* CE::Platform::DisplaySystem::create_window(const CE::Platform::Monitor& monitor, CE::Platform::WindowMode mode, int width, int height)
 * @brief Create a display-owned window on the platform owner.
 * @param monitor Recognized inventory snapshot.
 * @param mode Backend-supported initial window mode.
 * @param width Positive logical width.
 * @param height Positive logical height.
 * @return Borrowed window pointer; the display retains ownership.
 * Validation/native failures follow the backend; creating a window alone does not select an independent rendering/resource domain.
 * @see [Detailed guide](cheryl-guide:runtime/display-and-window-contract)
 */

/**
 * @fn void CE::Platform::DisplaySystem::activate_window(CE::Platform::Window& window)
 * @brief Select a display-owned window within the backend transition policy.
 * @param window Window owned by this display.
 * Selection does not promise renderer/context/resource-domain switching. NativeGLFW rejects replacement of an already selected different window.
 * @see [Detailed guide](cheryl-guide:runtime/display-and-window-contract)
 */
