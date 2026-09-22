#pragma once

#include <gainput/gainput.h>

namespace CE::Input {
    [[nodiscard]] gainput::DeviceButtonId gainput_key(int glfw_key);
    [[nodiscard]] gainput::DeviceButtonId gainput_mouse_button(int glfw_button);
}
