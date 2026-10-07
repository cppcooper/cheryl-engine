#pragma once

#include "monitor.h"
#include "window-interface.h"

#include <utility>
#include <vector>

namespace CE {
    /** Platform-owner display/window factory. Created windows remain display-owned;
     * callers borrow them and detach dependent input/render owners before teardown.
     * Monitor references are borrowed inventory snapshots with backend-defined refresh
     * and invalidation points. Copy values before handing them to another thread.
     */
    class iDisplaySystem {
    public:
        virtual ~iDisplaySystem() = default;
        /** Borrow the backend inventory; references expire on its refresh or destruction. */
        [[nodiscard]] virtual const std::vector<Monitor>& monitors() const = 0;
        [[nodiscard]] virtual int monitor_count() const = 0;
        /** Borrow the backend's primary-monitor snapshot under the same inventory lifetime. */
        [[nodiscard]] virtual const Monitor& primary_monitor() const = 0;
        /** Borrow the selected window; a backend without a selection may return nullptr. */
        [[nodiscard]] virtual iWindow* active_window() const = 0;
        /** Monitor X/Y scale for a snapshot recognized by this display. This is not
         * a per-window scale/change-notification contract; see the backend's policy.
         */
        [[nodiscard]] virtual std::pair<float, float> content_scale(const Monitor& monitor) const = 0;
        /** Create a display-owned window from a recognized monitor, supported mode
         * and positive logical dimensions. The returned pointer transfers no ownership.
         */
        virtual iWindow* create_window(const Monitor& monitor, Enum::window_mode mode, int width, int height) = 0;
        /** Select an owned window within the backend's supported transition policy.
         * Renderer/context/resource-domain switching requires its own graphics contract.
         */
        virtual void activate_window(iWindow& window) = 0;
    };
}
