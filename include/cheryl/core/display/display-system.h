#pragma once
#include "monitor.h"
#include "window.h"

#include <memory>
#include <vector>

namespace CE {
    struct Resolution {
        int width{};
        int height{};
    };

    // Tracks GLFW windows and monitor information. Rendering state belongs to the renderer.
    // Monitor modes are snapshots; callers can query GLFW again if monitors change at runtime.
    class DisplaySystem {
    public:
        DisplaySystem();

        [[nodiscard]] const std::vector<Monitor>& monitors() const { return monitors_; }
        [[nodiscard]] int monitor_count() const { return static_cast<int>(monitors_.size()); }
        [[nodiscard]] const Monitor& primary_monitor() const { return primary_monitor_; }
        [[nodiscard]] Window* active_window() const { return active_; }

        Window* create_window(const Monitor& monitor, Enum::window_mode mode, int width, int height);
        Window* create_window(const Monitor& monitor, Enum::window_mode mode, Resolution resolution);
        Window* create_window(const Monitor& monitor, Enum::window_mode mode);
        // Selects the initial render window. Additional rendering contexts need GPU resource management.
        void activate_window(Window& window);

    private:
        std::vector<Monitor> monitors_;
        std::vector<std::unique_ptr<Window>> windows_;
        Monitor primary_monitor_;
        Window* active_ = nullptr;
    };
}
