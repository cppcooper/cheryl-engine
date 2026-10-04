#pragma once
#include "display-system-interface.h"
#include "window.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

class GLFWmonitor;

namespace CE {
    struct Resolution {
        int width{};
        int height{};
    };

    // Owns GLFW and its windows. Monitor modes are snapshots; this backend can refresh
    // them if monitors change at runtime. Construct and destroy on the platform thread.
    class DisplaySystem final : public iDisplaySystem {
    public:
        DisplaySystem();

        [[nodiscard]] const std::vector<Monitor>& monitors() const override { return monitors_; }
        [[nodiscard]] int monitor_count() const override { return static_cast<int>(monitors_.size()); }
        [[nodiscard]] const Monitor& primary_monitor() const override { return primary_monitor_; }
        [[nodiscard]] Window* active_window() const override { return active_; }
        [[nodiscard]] std::pair<float, float> content_scale(const Monitor& monitor) const override;

        Window* create_window(const Monitor& monitor, Enum::window_mode mode, int width, int height) override;
        Window* create_window(const Monitor& monitor, Enum::window_mode mode, int width, int height, const std::string& title);
        Window* create_window(const Monitor& monitor, Enum::window_mode mode, Resolution resolution);
        Window* create_window(const Monitor& monitor, Enum::window_mode mode);
        // Selects the initial render window. Additional rendering contexts need GPU resource management.
        void activate_window(iWindow& window) override;

    private:
        struct GlfwLibrary {
            GlfwLibrary();
            ~GlfwLibrary();
            GlfwLibrary(const GlfwLibrary&) = delete;
            GlfwLibrary& operator=(const GlfwLibrary&) = delete;
        } glfw_;

        [[nodiscard]] static Monitor create_primary_monitor();
        [[nodiscard]] GLFWmonitor* native_monitor(const Monitor& monitor) const;

        std::vector<Monitor> monitors_;
        std::vector<GLFWmonitor*> native_monitors_;
        std::vector<std::unique_ptr<Window>> windows_;
        Monitor primary_monitor_;
        Window* active_ = nullptr;
    };
}
