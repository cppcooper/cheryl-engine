#define CHERYL_NATIVE_TEST_IMPLEMENTATION
#include "backends/native-glfw/test-access.h"

namespace CE::Platform::GLFW::Detail::Test {
    decltype(&::glfwCreateWindow) glfwCreateWindow_entry() noexcept {
        return &::glfwCreateWindow;
    }
    decltype(&::glfwDefaultWindowHints) glfwDefaultWindowHints_entry() noexcept {
        return &::glfwDefaultWindowHints;
    }
    decltype(&::glfwDestroyWindow) glfwDestroyWindow_entry() noexcept {
        return &::glfwDestroyWindow;
    }
    decltype(&::glfwGetCurrentContext) glfwGetCurrentContext_entry() noexcept {
        return &::glfwGetCurrentContext;
    }
    decltype(&::glfwGetPlatform) glfwGetPlatform_entry() noexcept {
        return &::glfwGetPlatform;
    }
    decltype(&::glfwMakeContextCurrent) glfwMakeContextCurrent_entry() noexcept {
        return &::glfwMakeContextCurrent;
    }
    decltype(&::glfwSetErrorCallback) glfwSetErrorCallback_entry() noexcept {
        return &::glfwSetErrorCallback;
    }
    decltype(&::glfwSetFramebufferSizeCallback) glfwSetFramebufferSizeCallback_entry() noexcept {
        return &::glfwSetFramebufferSizeCallback;
    }
    decltype(&::glfwWindowHint) glfwWindowHint_entry() noexcept {
        return &::glfwWindowHint;
    }
#if CHERYL_NATIVE_X11_TESTS
    decltype(&::glfwGetX11Display) glfwGetX11Display_entry() noexcept {
        return &::glfwGetX11Display;
    }
    decltype(&::glfwGetX11Window) glfwGetX11Window_entry() noexcept {
        return &::glfwGetX11Window;
    }
#endif
}
