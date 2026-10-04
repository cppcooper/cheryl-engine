#pragma once

#include "monitor.h"
#include "window-interface.h"

#include <utility>
#include <vector>

namespace CE {
    // Owns the windows it creates and supplies monitor snapshots for a display backend.
    class iDisplaySystem {
    public:
        virtual ~iDisplaySystem() = default;
        [[nodiscard]] virtual const std::vector<Monitor>& monitors() const = 0;
        [[nodiscard]] virtual int monitor_count() const = 0;
        [[nodiscard]] virtual const Monitor& primary_monitor() const = 0;
        [[nodiscard]] virtual iWindow* active_window() const = 0;
        [[nodiscard]] virtual std::pair<float, float> content_scale(const Monitor& monitor) const = 0;
        virtual iWindow* create_window(const Monitor& monitor, Enum::window_mode mode, int width, int height) = 0;
        virtual void activate_window(iWindow& window) = 0;
    };
}
