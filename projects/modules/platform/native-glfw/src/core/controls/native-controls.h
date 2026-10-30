#pragma once

#include <core/controls/button-codes.h>

namespace CE::Platform::GLFW::Detail {
    [[nodiscard]] CE::Input::KeyboardKey keyboard_key(int glfw_key);
    [[nodiscard]] CE::Input::MouseButton mouse_button(int glfw_button);
    [[nodiscard]] CE::Input::ControlId gainput_keyboard_control(int glfw_key);
    [[nodiscard]] CE::Input::ControlId gainput_mouse_control(int glfw_button);
    [[nodiscard]] CE::Input::ControlId gamepad_control(CE::Input::ControlId gainput_control);
}
