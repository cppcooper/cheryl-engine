#pragma once
#include "framebuffer-size.h"
#include "monitor.h"
#include "viewport.h"

#include <enums.h>

class GLFWwindow;
class GLFWmonitor;

namespace CE {
    // Owns a GLFW window and its logical and framebuffer dimensions.
    class Window final {
    public:
        ~Window();
        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;
        Window(Window&&) = delete;
        Window& operator=(Window&&) = delete;

        [[nodiscard]] GLFWwindow* native_handle() const { return glfw_window_; }
        [[nodiscard]] ViewPort<int> logical_size() const { return logical_size_; }
        [[nodiscard]] FramebufferSize framebuffer_size() const { return framebuffer_size_; }
        [[nodiscard]] Enum::window_mode mode() const { return window_mode_; }

        void resize(int width, int height);
        void set_mode(Enum::window_mode mode);
        void hide_cursor(bool hide) const;

    private:
        friend class DisplaySystem;
        Window(const Monitor& monitor, GLFWmonitor* native_monitor, Enum::window_mode mode, int width, int height);

        static void on_window_size(GLFWwindow* window, int width, int height);
        static void on_framebuffer_size(GLFWwindow* window, int width, int height);
        void update_framebuffer_size(int width, int height);

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
    };
}
