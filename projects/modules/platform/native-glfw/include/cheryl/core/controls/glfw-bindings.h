#pragma once

#if CHERYL_NATIVE_INPUT
#include <core/controls/button-codes.h>
#include <gainput/gainput.h>

namespace CE::Input {
    [[nodiscard]] KeyboardKey keyboard_key(int glfw_key);
    [[nodiscard]] MouseButton mouse_button(int glfw_button);
    [[nodiscard]] gainput::DeviceButtonId gainput_key(int glfw_key);
    [[nodiscard]] gainput::DeviceButtonId gainput_mouse_button(int glfw_button);
}
#endif
