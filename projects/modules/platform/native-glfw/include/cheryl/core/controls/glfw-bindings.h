#pragma once

#include <cheryl/export.h>

#if CHERYL_NATIVE_INPUT
#include <core/controls/button-codes.h>
#include <gainput/gainput.h>

namespace CE::Platform::GLFW {
    [[nodiscard]] CE_NATIVE_GLFW_API CE::Input::KeyboardKey keyboard_key(int glfw_key);
    [[nodiscard]] CE_NATIVE_GLFW_API CE::Input::MouseButton mouse_button(int glfw_button);
    [[nodiscard]] CE_NATIVE_GLFW_API gainput::DeviceButtonId gainput_key(int glfw_key);
    [[nodiscard]] CE_NATIVE_GLFW_API gainput::DeviceButtonId gainput_mouse_button(int glfw_button);
}
#endif
