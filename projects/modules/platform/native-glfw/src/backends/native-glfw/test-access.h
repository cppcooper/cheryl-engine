#pragma once

#include <cheryl/export.h>
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#if CHERYL_NATIVE_X11_TESTS
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#undef None
#endif

namespace CE::Platform::GLFW::Detail::Test {
    [[nodiscard]] CE_NATIVE_GLFW_API decltype(&::glfwCreateWindow) glfwCreateWindow_entry() noexcept;
    [[nodiscard]] CE_NATIVE_GLFW_API decltype(&::glfwDefaultWindowHints) glfwDefaultWindowHints_entry() noexcept;
    [[nodiscard]] CE_NATIVE_GLFW_API decltype(&::glfwDestroyWindow) glfwDestroyWindow_entry() noexcept;
    [[nodiscard]] CE_NATIVE_GLFW_API decltype(&::glfwGetCurrentContext) glfwGetCurrentContext_entry() noexcept;
    [[nodiscard]] CE_NATIVE_GLFW_API decltype(&::glfwGetPlatform) glfwGetPlatform_entry() noexcept;
    [[nodiscard]] CE_NATIVE_GLFW_API decltype(&::glfwMakeContextCurrent) glfwMakeContextCurrent_entry() noexcept;
    [[nodiscard]] CE_NATIVE_GLFW_API decltype(&::glfwSetErrorCallback) glfwSetErrorCallback_entry() noexcept;
    [[nodiscard]] CE_NATIVE_GLFW_API decltype(&::glfwSetFramebufferSizeCallback) glfwSetFramebufferSizeCallback_entry() noexcept;
    [[nodiscard]] CE_NATIVE_GLFW_API decltype(&::glfwWindowHint) glfwWindowHint_entry() noexcept;
#if CHERYL_NATIVE_X11_TESTS
    [[nodiscard]] CE_NATIVE_GLFW_API decltype(&::glfwGetX11Display) glfwGetX11Display_entry() noexcept;
    [[nodiscard]] CE_NATIVE_GLFW_API decltype(&::glfwGetX11Window) glfwGetX11Window_entry() noexcept;
#endif
}

#if CHERYL_NATIVE_INPUT
#include <core/controls/input-system.h>
namespace CE::Platform::GLFW::Detail {
    struct InputTestAccess {
        [[nodiscard]] static CE_NATIVE_GLFW_API CE::Input::KeyboardKey keyboard_key(int key);
        [[nodiscard]] static CE_NATIVE_GLFW_API CE::Input::MouseButton mouse_button(int button);
        static CE_NATIVE_GLFW_API void use_external_state(InputSystem& input, CE::Input::DeviceId device);
        [[nodiscard]] static CE_NATIVE_GLFW_API bool
        notify_button(InputSystem& input, CE::Input::DeviceId device, CE::Input::ControlId control, bool old_value, bool new_value);
        [[nodiscard]] static CE_NATIVE_GLFW_API bool
        notify_axis(InputSystem& input, CE::Input::DeviceId device, CE::Input::ControlId control, float old_value, float new_value);
    };
}
#endif

#ifndef CHERYL_NATIVE_TEST_IMPLEMENTATION
#define glfwCreateWindow (*::CE::Platform::GLFW::Detail::Test::glfwCreateWindow_entry())
#define glfwDefaultWindowHints (*::CE::Platform::GLFW::Detail::Test::glfwDefaultWindowHints_entry())
#define glfwDestroyWindow (*::CE::Platform::GLFW::Detail::Test::glfwDestroyWindow_entry())
#define glfwGetCurrentContext (*::CE::Platform::GLFW::Detail::Test::glfwGetCurrentContext_entry())
#define glfwGetPlatform (*::CE::Platform::GLFW::Detail::Test::glfwGetPlatform_entry())
#define glfwMakeContextCurrent (*::CE::Platform::GLFW::Detail::Test::glfwMakeContextCurrent_entry())
#define glfwSetErrorCallback (*::CE::Platform::GLFW::Detail::Test::glfwSetErrorCallback_entry())
#define glfwSetFramebufferSizeCallback (*::CE::Platform::GLFW::Detail::Test::glfwSetFramebufferSizeCallback_entry())
#define glfwWindowHint (*::CE::Platform::GLFW::Detail::Test::glfwWindowHint_entry())
#if CHERYL_NATIVE_X11_TESTS
#define glfwGetX11Display (*::CE::Platform::GLFW::Detail::Test::glfwGetX11Display_entry())
#define glfwGetX11Window (*::CE::Platform::GLFW::Detail::Test::glfwGetX11Window_entry())
#endif
#endif
