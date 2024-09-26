#include <engine/view/window.h>
#include <internals.h>

#include <GLFW/glfw3.h>
#include <random>

const char* generate_title();
inline GLFWwindow* create_window(const CE::Monitor&, CE::Enum::window_mode, uint16_t, uint16_t);

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    glfwMakeContextCurrent(window);
    glViewport(0,0,width,height);
}

namespace CE {
    Window::Window(const Monitor& monitor, const Enum::window_mode mode, const uint16_t width, const uint16_t height)
        : ViewPort(width, height), xpos(0), ypos(0), window_mode(mode),
          glfw_window(create_window(monitor, mode, width, height)),
          monitor(monitor) {
        glfwGetMonitorPos(monitor.glfw_monitor, &xpos, &ypos);
        if (mode == Enum::window_mode::NORMAL) {
            glfwSetWindowPos(glfw_window, xpos, ypos);
        }
        glfwSetFramebufferSizeCallback(glfw_window, framebuffer_size_callback);
    }

    void Window::activate() const {
        glfwMakeContextCurrent(glfw_window);
    }

    void Window::resize(const uint16_t width, const uint16_t height) {
        if (glfw_window) {
            x = width;
            y = height;
            glfwSetWindowSize(glfw_window, width, height);
        }
    }

    void Window::set_mode(Enum::window_mode mode) {
        if (window_mode == mode) {
            return;
        }
        const GLFWvidmode* vidmode = glfwGetVideoMode(monitor.glfw_monitor);
        if (!vidmode) {
            CELog::critical("Unable to retrieve GLFWvidemode* from glfwGetVideoMode({})", reinterpret_cast<uint64_t>(monitor.glfw_monitor));
            throw Exceptions::failed_operation(CE_HERE, "glfwGetVideoMode() failed to return the video mode.");
        }
        uint16_t w{256},h{256};
        switch(mode) {
            case Enum::window_mode::NORMAL: {
                glfwWindowHint(GLFW_DECORATED, true);
                glfwSetWindowMonitor(glfw_window, nullptr, xpos, ypos, width, height, 0);
                w=width; h=height;
                break;
            }
            case Enum::window_mode::BORDERLESS: {
                glfwWindowHint(GLFW_DECORATED, false);
                glfwWindowHint(GLFW_RED_BITS, vidmode->redBits);
                glfwWindowHint(GLFW_GREEN_BITS, vidmode->greenBits);
                glfwWindowHint(GLFW_BLUE_BITS, vidmode->blueBits);
                glfwWindowHint(GLFW_REFRESH_RATE, vidmode->refreshRate);
                glfwSetWindowMonitor(glfw_window, nullptr, xpos, ypos, width, height, 0);
                w=width; h=height;
                break;
            }
            case Enum::window_mode::FULLSCREEN: {
                glfwGetWindowPos(glfw_window, &xpos, &ypos);
                glfwSetWindowMonitor(glfw_window, monitor.glfw_monitor, 0, 0, monitor.width, monitor.height, vidmode->refreshRate);
                w=monitor.width; h=monitor.height;
                break;
            }
        }
        activate();
        glViewport(0,0,w,h);
    }

    void Window::hide_cursor(bool hide) const {
        if (!hide) {
            glfwSetInputMode(glfw_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL );
        } else {
            glfwSetInputMode(glfw_window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN );
        }
    }
}

using CE::Monitor;
namespace Enum = CE::Enum;
inline GLFWwindow* create_window(const Monitor& monitor, const Enum::window_mode mode, const uint16_t width, const uint16_t height) {
        switch (mode) {
            case Enum::window_mode::NORMAL: {
                glfwWindowHint(GLFW_DECORATED, true);
                auto w = glfwCreateWindow(width, height, generate_title(), nullptr, nullptr);
                glfwMakeContextCurrent(w);
                glViewport(0,0,width,height);
                return w;
            }
            case Enum::window_mode::FULLSCREEN:
                return glfwCreateWindow(width, height, generate_title(), monitor.glfw_monitor, nullptr);
            case Enum::window_mode::BORDERLESS:
                const GLFWvidmode* vidmode = glfwGetVideoMode(monitor.glfw_monitor);
            glfwWindowHint(GLFW_DECORATED, false);
            glfwWindowHint(GLFW_RED_BITS, vidmode->redBits);
            glfwWindowHint(GLFW_GREEN_BITS, vidmode->greenBits);
            glfwWindowHint(GLFW_BLUE_BITS, vidmode->blueBits);
            glfwWindowHint(GLFW_REFRESH_RATE, vidmode->refreshRate);
            return glfwCreateWindow(width, height, generate_title(), nullptr, nullptr);
        }
        return nullptr;
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
