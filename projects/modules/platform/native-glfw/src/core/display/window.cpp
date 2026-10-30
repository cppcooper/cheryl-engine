#include <core/display/window.h>
#include "glfw-diagnostics.h"

#include <core/subsystems/event-system.h>
#include <internals/exceptions.h>
#include <internals/failure-reporting.h>
#include <internals/compile-time-logging.hpp>

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#include <random>
#include <string>
#include <utility>

namespace {
    const char* generate_title();
    GLFWwindow* create_native_window(GLFWmonitor*, ::CE::Platform::WindowMode, int, int, const std::string&);
}

namespace CE::Platform::GLFW {
    Window::Window(
        const ::CE::Platform::Monitor& monitor,
        GLFWmonitor* native_monitor,
        const ::CE::Platform::WindowMode mode,
        const int width,
        const int height,
        const std::string& title
    )
    : logical_size_(width, height),
      window_mode_(mode),
      monitor_(monitor),
      glfw_monitor_(native_monitor),
      glfw_window_(create_native_window(native_monitor, mode, width, height, title)),
      windowed_width_(width),
      windowed_height_(height) {
        glfwGetMonitorPos(glfw_monitor_, &windowed_x_, &windowed_y_);
        if (mode == ::CE::Platform::WindowMode::NORMAL) {
            glfwSetWindowPos(glfw_window_, windowed_x_, windowed_y_);
        }
        glfwGetWindowSize(glfw_window_, &logical_size_.width, &logical_size_.height);
        glfwGetFramebufferSize(glfw_window_, &framebuffer_size_.width, &framebuffer_size_.height);
        // Track logical and pixel sizes separately; GLFW callbacks may subsequently change either.
        glfwSetWindowUserPointer(glfw_window_, this);
        glfwSetWindowSizeCallback(glfw_window_, on_window_size);
        glfwSetFramebufferSizeCallback(glfw_window_, on_framebuffer_size);
        CE_LOG_INFO(
            ::CE::Logging::Categories::platform,
            "subsystem=window domain={} operation=create mode={} logical_width={} logical_height={} pixel_width={} pixel_height={}",
            domain_, static_cast<int>(window_mode_), logical_size_.width, logical_size_.height, framebuffer_size_.width,
            framebuffer_size_.height
        );
    }

    Window::~Window() {
        glfwSetFramebufferSizeCallback(glfw_window_, nullptr);
        glfwSetWindowSizeCallback(glfw_window_, nullptr);
        glfwSetWindowUserPointer(glfw_window_, nullptr);
        glfwDestroyWindow(glfw_window_);
        if (native_failure_)
            ::CE::Diagnostics::report_failure("window destruction with an unconsumed callback failure", native_failure_);
    }

    std::optional<bool> Window::focused() const {
        return glfwGetWindowAttrib(glfw_window_, GLFW_FOCUSED) == GLFW_TRUE;
    }

    bool Window::consume_close_intent() {
        if (glfwWindowShouldClose(glfw_window_) != GLFW_TRUE)
            return false;
        glfwSetWindowShouldClose(glfw_window_, GLFW_FALSE);
        return true;
    }

    void Window::make_context_current(const int swap_interval) {
        if (swap_interval < 0)
            throw ::CE::Exceptions::invalid_args(CE_HERE, "Swap interval must not be negative");
        glfwMakeContextCurrent(glfw_window_);
        if (!context_current()) {
            Detail::report_glfw_diagnostics("make_current", true);
            throw ::CE::Exceptions::failed_operation(CE_HERE, "Could not make the window's OpenGL context current");
        }
        Detail::report_glfw_diagnostics("make_current");
        glfwSwapInterval(swap_interval);
    }

    void Window::release_context() {
        if (!context_current())
            throw ::CE::Exceptions::failed_operation(CE_HERE, "The window's OpenGL context is not current on this thread");
        glfwMakeContextCurrent(nullptr);
    }

    bool Window::context_current() const {
        return glfwGetCurrentContext() == glfw_window_;
    }

    Window::ProcAddress Window::context_proc_address(const char* name) const {
        if (!name)
            throw ::CE::Exceptions::invalid_args(CE_HERE, "OpenGL procedure name must not be null");
        if (!context_current())
            throw ::CE::Exceptions::failed_operation(CE_HERE, "OpenGL procedure lookup requires the current context");
        return glfwGetProcAddress(name);
    }

    void Window::swap_buffers() {
        if (!context_current())
            throw ::CE::Exceptions::failed_operation(CE_HERE, "Present requires the current OpenGL context");
        glfwSwapBuffers(glfw_window_);
    }

    void Window::on_window_size(GLFWwindow* handle, const int width, const int height) noexcept {
        Detail::NativeCallbackScope callback_scope;
        auto* window = static_cast<Window*>(glfwGetWindowUserPointer(handle));
        if (!window)
            return;
        window->logical_size_ = {width, height};
        ++window->resize_observations_;
        if (window->window_mode_ == ::CE::Platform::WindowMode::NORMAL) {
            window->windowed_width_ = width;
            window->windowed_height_ = height;
        }
    }

    void Window::on_framebuffer_size(GLFWwindow* handle, const int width, const int height) noexcept {
        Detail::NativeCallbackScope callback_scope;
        auto* window = static_cast<Window*>(glfwGetWindowUserPointer(handle));
        if (!window)
            return;
        if (window->native_failure_) {
            // Keep the latest dimensions but suppress further listener work until
            // the platform owner consumes the first failure.
            window->framebuffer_size_ = {width, height};
            return;
        }
        try {
            window->update_framebuffer_size(width, height);
        } catch (...) {
            window->native_failure_ = std::current_exception();
        }
    }

    void Window::check_native_failure() const {
        Detail::report_glfw_diagnostics("platform_observation");
        if (native_failure_) {
            if (!Detail::NativeCallbackScope::active())
                ::CE::Diagnostics::report_outcome("window", domain_, "framebuffer_callback", "failed");
            std::rethrow_exception(std::exchange(native_failure_, {}));
        }
        if (Detail::NativeCallbackScope::active())
            return;
        const auto now = std::chrono::steady_clock::now();
        if (resize_observations_ && now >= next_diagnostic_) {
            const auto count = std::exchange(resize_observations_, 0);
            next_diagnostic_ = now + std::chrono::seconds{2};
            CE_LOG_DEBUG(
                ::CE::Logging::Categories::platform,
                "subsystem=window domain={} operation=resize observations={} logical_width={} logical_height={} pixel_width={} pixel_height={}",
                domain_, count, logical_size_.width, logical_size_.height, framebuffer_size_.width, framebuffer_size_.height
            );
        }
    }

    void Window::update_framebuffer_size(const int width, const int height) {
        if (framebuffer_size_ == ::CE::Platform::FramebufferSize{width, height})
            return;
        framebuffer_size_ = {width, height};
        ++resize_observations_;
        // Consumers recompute pixel-dependent state (for example, camera projection) on this event.
        // Keep one observation across both channels, even if a legacy listener
        // changes the window again. Legacy delivery keeps its existing order/failure boundary.
        const ::CE::Platform::WindowResized resized{this, framebuffer_size_};
        auto& events = ::CE::Events::EventSystem::get();
        events.dispatch("window-resized", resized);
        events.dispatch(::CE::Platform::window_resized_event, resized);
    }

    void Window::resize(const int width, const int height) {
        check_native_failure();
        if (width <= 0 || height <= 0)
            throw ::CE::Exceptions::invalid_args(CE_HERE, "Window dimensions must be positive");
        glfwSetWindowSize(glfw_window_, width, height);
        // Query the accepted logical size and remember it only for ordinary
        // windows; fullscreen restoration uses the last windowed dimensions.
        glfwGetWindowSize(glfw_window_, &logical_size_.width, &logical_size_.height);
        if (window_mode_ == ::CE::Platform::WindowMode::NORMAL) {
            windowed_width_ = logical_size_.width;
            windowed_height_ = logical_size_.height;
        }
        int framebuffer_width = 0;
        int framebuffer_height = 0;
        // A logical resize can yield a different pixel size on scaled displays.
        glfwGetFramebufferSize(glfw_window_, &framebuffer_width, &framebuffer_height);
        check_native_failure();
        update_framebuffer_size(framebuffer_width, framebuffer_height);
    }

    void Window::set_mode(const ::CE::Platform::WindowMode mode) {
        check_native_failure();
        if (window_mode_ == mode)
            return;
        if (mode != ::CE::Platform::WindowMode::NORMAL && mode != ::CE::Platform::WindowMode::BORDERLESS &&
            mode != ::CE::Platform::WindowMode::FULLSCREEN)
            throw ::CE::Exceptions::invalid_args(CE_HERE, "Unknown window mode");

        const GLFWvidmode* vidmode = glfwGetVideoMode(glfw_monitor_);
        if (!vidmode) {
            throw ::CE::Exceptions::failed_operation(CE_HERE, "glfwGetVideoMode() failed to return the video mode");
        }

        // Save the ordinary window placement before changing modes so NORMAL can restore it.
        if (window_mode_ == ::CE::Platform::WindowMode::NORMAL) {
            glfwGetWindowPos(glfw_window_, &windowed_x_, &windowed_y_);
            glfwGetWindowSize(glfw_window_, &windowed_width_, &windowed_height_);
        }
        window_mode_ = mode;
        if (!Detail::NativeCallbackScope::active())
            CE_LOG_DEBUG(
                ::CE::Logging::Categories::platform, "subsystem=window domain={} operation=mode value={}", domain_, static_cast<int>(mode)
            );
        // Windowed and borderless use a detached monitor; fullscreen attaches
        // the selected monitor at its video mode and refresh rate.
        switch (mode) {
            case ::CE::Platform::WindowMode::NORMAL:
                glfwSetWindowMonitor(glfw_window_, nullptr, windowed_x_, windowed_y_, windowed_width_, windowed_height_, 0);
                glfwSetWindowAttrib(glfw_window_, GLFW_DECORATED, GLFW_TRUE);
                break;
            case ::CE::Platform::WindowMode::BORDERLESS:
                glfwSetWindowMonitor(glfw_window_, nullptr, windowed_x_, windowed_y_, windowed_width_, windowed_height_, 0);
                glfwSetWindowAttrib(glfw_window_, GLFW_DECORATED, GLFW_FALSE);
                break;
            case ::CE::Platform::WindowMode::FULLSCREEN:
                glfwSetWindowMonitor(glfw_window_, glfw_monitor_, 0, 0, monitor_.width, monitor_.height, vidmode->refreshRate);
                break;
        }
        // Mode switches can change framebuffer size independently of logical window size.
        glfwGetWindowSize(glfw_window_, &logical_size_.width, &logical_size_.height);
        int framebuffer_width = 0;
        int framebuffer_height = 0;
        glfwGetFramebufferSize(glfw_window_, &framebuffer_width, &framebuffer_height);
        check_native_failure();
        update_framebuffer_size(framebuffer_width, framebuffer_height);
    }

    void Window::hide_cursor(const bool hide) const {
        glfwSetInputMode(glfw_window_, GLFW_CURSOR, hide ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_NORMAL);
    }

    bool Window::should_close() const {
        check_native_failure();
        return glfwWindowShouldClose(glfw_window_) == GLFW_TRUE;
    }
}

namespace {
    GLFWwindow* create_native_window(
        GLFWmonitor* monitor,
        const ::CE::Platform::WindowMode mode,
        const int width,
        const int height,
        const std::string& title
    ) {
        if (width <= 0 || height <= 0)
            throw ::CE::Exceptions::invalid_args(CE_HERE, "Window dimensions must be positive");
        if (mode != ::CE::Platform::WindowMode::NORMAL && mode != ::CE::Platform::WindowMode::BORDERLESS &&
            mode != ::CE::Platform::WindowMode::FULLSCREEN)
            throw ::CE::Exceptions::invalid_args(CE_HERE, "Unknown window mode");

        // The initial mode decides decoration and whether GLFW attaches the monitor.
        glfwWindowHint(GLFW_DECORATED, mode == ::CE::Platform::WindowMode::NORMAL ? GLFW_TRUE : GLFW_FALSE);
        auto* fullscreen_monitor = mode == ::CE::Platform::WindowMode::FULLSCREEN ? monitor : nullptr;
        auto* native = glfwCreateWindow(width, height, title.empty() ? generate_title() : title.c_str(), fullscreen_monitor, nullptr);
        if (!native) {
            ::CE::Platform::GLFW::Detail::report_glfw_diagnostics("create_window", true);
            throw ::CE::Exceptions::runtime_exception(CE_HERE, "Failed to create a GLFW window");
        }
        return native;
    }

    const char* generate_title() {
        std::random_device rng;
        std::uniform_int_distribution<> uid(1, 20);
        switch (uid(rng)) {
            case 1:
                return "One is the loneliest number.";
            case 2:
                return "Two's company, three's a crowd.";
            case 3:
                return "Three's a charm!";
            case 4:
                return "Four-leaf clover, lucky number!";
            case 5:
                return "High five!";
            case 6:
                return "Six feet under.";
            case 7:
                return "Lucky number seven!";
            case 8:
                return "Eight ball, corner pocket.";
            case 9:
                return "Nine lives, like a cat!";
            case 10:
                return "Perfect ten!";
            case 11:
                return "Eleven pipers piping!";
            case 12:
                return "Twelve days of Christmas!";
            case 13:
                return "Unlucky thirteen!";
            case 14:
                return "Fourteen karat gold.";
            case 15:
                return "Fifteen minutes of fame!";
            case 16:
                return "Sweet sixteen!";
            case 17:
                return "Seventeen candles on the cake.";
            case 18:
                return "Eighteen holes on the golf course.";
            case 19:
                return "Nineteen is prime!";
            case 20:
                return "Twenty questions!";
            default:
                return "Out of range!";
        }
    }
}
