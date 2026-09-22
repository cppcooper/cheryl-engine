#include <core/display/window.h>

#include <core/subsystems/event-system.h>
#include <internals/exceptions.h>

#include <GLFW/glfw3.h>
#include <random>
#include <tuple>

const char* generate_title();
GLFWwindow* create_native_window(GLFWmonitor*, CE::Enum::window_mode, int, int);

namespace CE {
    Window::Window(const Monitor& monitor, GLFWmonitor* native_monitor, const Enum::window_mode mode, const int width,
                   const int height) :
        logical_size_(width, height), window_mode_(mode), monitor_(monitor), glfw_monitor_(native_monitor),
        glfw_window_(create_native_window(native_monitor, mode, width, height)), windowed_width_(width),
        windowed_height_(height) {
        glfwGetMonitorPos(glfw_monitor_, &windowed_x_, &windowed_y_);
        if (mode == Enum::window_mode::NORMAL) {
            glfwSetWindowPos(glfw_window_, windowed_x_, windowed_y_);
        }
        glfwGetWindowSize(glfw_window_, &logical_size_.width, &logical_size_.height);
        glfwGetFramebufferSize(glfw_window_, &framebuffer_size_.width, &framebuffer_size_.height);
        glfwSetWindowUserPointer(glfw_window_, this);
        glfwSetWindowSizeCallback(glfw_window_, on_window_size);
        glfwSetFramebufferSizeCallback(glfw_window_, on_framebuffer_size);
    }

    Window::~Window() {
        glfwSetFramebufferSizeCallback(glfw_window_, nullptr);
        glfwSetWindowSizeCallback(glfw_window_, nullptr);
        glfwSetWindowUserPointer(glfw_window_, nullptr);
        glfwDestroyWindow(glfw_window_);
    }

    void Window::on_window_size(GLFWwindow* handle, const int width, const int height) {
        auto* window = static_cast<Window*>(glfwGetWindowUserPointer(handle));
        if (!window)
            return;
        window->logical_size_ = {width, height};
        if (window->window_mode_ == Enum::window_mode::NORMAL) {
            window->windowed_width_ = width;
            window->windowed_height_ = height;
        }
    }

    void Window::on_framebuffer_size(GLFWwindow* handle, const int width, const int height) {
        auto* window = static_cast<Window*>(glfwGetWindowUserPointer(handle));
        if (window)
            window->update_framebuffer_size(width, height);
    }

    void Window::update_framebuffer_size(const int width, const int height) {
        if (framebuffer_size_ == FramebufferSize{width, height})
            return;
        framebuffer_size_ = {width, height};
        SubSystems::EventSystem::get().dispatch("window-resized", std::make_tuple(glfw_window_, width, height));
    }

    void Window::resize(const int width, const int height) {
        if (width <= 0 || height <= 0)
            throw Exceptions::invalid_args(CE_HERE, "Window dimensions must be positive");
        glfwSetWindowSize(glfw_window_, width, height);
        glfwGetWindowSize(glfw_window_, &logical_size_.width, &logical_size_.height);
        if (window_mode_ == Enum::window_mode::NORMAL) {
            windowed_width_ = logical_size_.width;
            windowed_height_ = logical_size_.height;
        }
        int framebuffer_width = 0;
        int framebuffer_height = 0;
        glfwGetFramebufferSize(glfw_window_, &framebuffer_width, &framebuffer_height);
        update_framebuffer_size(framebuffer_width, framebuffer_height);
    }

    void Window::set_mode(const Enum::window_mode mode) {
        if (window_mode_ == mode)
            return;
        if (mode != Enum::window_mode::NORMAL && mode != Enum::window_mode::BORDERLESS &&
            mode != Enum::window_mode::FULLSCREEN)
            throw Exceptions::invalid_args(CE_HERE, "Unknown window mode");

        const GLFWvidmode* vidmode = glfwGetVideoMode(glfw_monitor_);
        if (!vidmode) {
            throw Exceptions::failed_operation(CE_HERE, "glfwGetVideoMode() failed to return the video mode");
        }

        if (window_mode_ == Enum::window_mode::NORMAL) {
            glfwGetWindowPos(glfw_window_, &windowed_x_, &windowed_y_);
            glfwGetWindowSize(glfw_window_, &windowed_width_, &windowed_height_);
        }
        window_mode_ = mode;
        switch (mode) {
        case Enum::window_mode::NORMAL:
            glfwSetWindowMonitor(glfw_window_, nullptr, windowed_x_, windowed_y_, windowed_width_, windowed_height_, 0);
            glfwSetWindowAttrib(glfw_window_, GLFW_DECORATED, GLFW_TRUE);
            break;
        case Enum::window_mode::BORDERLESS:
            glfwSetWindowMonitor(glfw_window_, nullptr, windowed_x_, windowed_y_, windowed_width_, windowed_height_, 0);
            glfwSetWindowAttrib(glfw_window_, GLFW_DECORATED, GLFW_FALSE);
            break;
        case Enum::window_mode::FULLSCREEN:
            glfwSetWindowMonitor(glfw_window_, glfw_monitor_, 0, 0, monitor_.width, monitor_.height,
                                 vidmode->refreshRate);
            break;
        }
        glfwGetWindowSize(glfw_window_, &logical_size_.width, &logical_size_.height);
        int framebuffer_width = 0;
        int framebuffer_height = 0;
        glfwGetFramebufferSize(glfw_window_, &framebuffer_width, &framebuffer_height);
        update_framebuffer_size(framebuffer_width, framebuffer_height);
    }

    void Window::hide_cursor(const bool hide) const {
        glfwSetInputMode(glfw_window_, GLFW_CURSOR, hide ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_NORMAL);
    }
}

namespace Enum = CE::Enum;
GLFWwindow* create_native_window(GLFWmonitor* monitor, const Enum::window_mode mode, const int width,
                                 const int height) {
    if (width <= 0 || height <= 0)
        throw CE::Exceptions::invalid_args(CE_HERE, "Window dimensions must be positive");
    if (mode != Enum::window_mode::NORMAL && mode != Enum::window_mode::BORDERLESS &&
        mode != Enum::window_mode::FULLSCREEN)
        throw CE::Exceptions::invalid_args(CE_HERE, "Unknown window mode");

    glfwWindowHint(GLFW_DECORATED, mode == Enum::window_mode::NORMAL ? GLFW_TRUE : GLFW_FALSE);
    auto* fullscreen_monitor = mode == Enum::window_mode::FULLSCREEN ? monitor : nullptr;
    auto* native = glfwCreateWindow(width, height, generate_title(), fullscreen_monitor, nullptr);
    if (!native)
        throw CE::Exceptions::runtime_exception(CE_HERE, "Failed to create a GLFW window");
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
