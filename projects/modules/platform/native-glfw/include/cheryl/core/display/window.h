#pragma once
#include <core/display/monitor.h>
#include <core/display/window-interface.h>

#include <string>
#include <exception>
#include <core/diagnostics.h>
#include <chrono>

class GLFWwindow;
class GLFWmonitor;

namespace CE {
    /** Owns a GLFW window and its logical/framebuffer dimensions on the platform
     * thread. Native callbacks retain their first failure; normal polling and
     * explicit resize/mode boundaries consume it through check_native_failure().
     * Size getters return cached observations updated by callbacks and explicit
     * resize/mode queries; they neither pump events nor synchronize cross-thread reads.
     * Input and graphics users release this borrowed native window before destruction.
     */
    class Window final : public iWindow {
        ViewPort<int> logical_size_;
        FramebufferSize framebuffer_size_{};
        Enum::window_mode window_mode_;
        Monitor monitor_;
        GLFWmonitor* glfw_monitor_;
        GLFWwindow* glfw_window_;
        int windowed_x_ = 0;
        int windowed_y_ = 0;
        int windowed_width_;
        int windowed_height_;
        mutable std::exception_ptr native_failure_;
        const Diagnostics::DomainId domain_ = Diagnostics::next_domain_id();
        mutable std::uint64_t resize_observations_ = 0;
        mutable std::chrono::steady_clock::time_point next_diagnostic_;

    public:
        ~Window() override;
        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;
        Window(Window&&) = delete;
        Window& operator=(Window&&) = delete;

        // Borrowed until destruction. Preserve Window's GLFW user pointer and callbacks.
        [[nodiscard]] GLFWwindow* native_handle() const { return glfw_window_; }
        [[nodiscard]] Diagnostics::DomainId diagnostic_id() const noexcept { return domain_; }
        [[nodiscard]] ViewPort<int> logical_size() const override { return logical_size_; }
        [[nodiscard]] FramebufferSize framebuffer_size() const override { return framebuffer_size_; }
        // Selected mode; a later callback failure does not roll this value back.
        [[nodiscard]] Enum::window_mode mode() const override { return window_mode_; }
        [[nodiscard]] bool should_close() const override;
        void check_native_failure() const override;

        /** Request positive logical dimensions and refresh observed logical/pixel sizes.
         * Listener failures can propagate after dimensions change; mutation is not atomic.
         */
        void resize(int width, int height) override;
        /** NORMAL restores saved windowed placement; BORDERLESS uses it undecorated;
         * FULLSCREEN attaches the selected monitor at its captured dimensions. Refresh
         * observed sizes afterward. Unknown modes fail; callback failures do not roll back.
         */
        void set_mode(Enum::window_mode mode) override;
        void hide_cursor(bool hide) const override;

    private:
        friend class DisplaySystem;
        Window(
            const Monitor& monitor,
            GLFWmonitor* native_monitor,
            Enum::window_mode mode,
            int width,
            int height,
            const std::string& title
        );

        static void on_window_size(GLFWwindow* window, int width, int height) noexcept;
        static void on_framebuffer_size(GLFWwindow* window, int width, int height) noexcept;
        void update_framebuffer_size(int width, int height);

    };
}
