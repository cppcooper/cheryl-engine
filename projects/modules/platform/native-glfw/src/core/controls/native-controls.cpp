#include "native-controls.h"

#include <gainput/gainput.h>

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <array>

namespace {
    struct KeyMapping {
        int native;
        CE::Input::KeyboardKey key;
        gainput::DeviceButtonId gainput;
    };

    struct PadMapping {
        gainput::DeviceButtonId native;
        CE::Input::GamepadControl control;
    };

#include "generated-controls.inc"

    constexpr auto key_lookup = [] {
        std::array<const KeyMapping*, GLFW_KEY_LAST + 1> keys{};
        for (const auto& mapping : key_mappings)
            keys[mapping.native] = &mapping;
        return keys;
    }();

    constexpr auto pad_lookup = [] {
        std::array<CE::Input::ControlId, gainput::PadButtonMax_> controls{};
        controls.fill(CE::Input::invalid_control);
        for (const auto& mapping : pad_mappings)
            controls[mapping.native] = CE::Input::control_id(mapping.control);
        return controls;
    }();
}

namespace CE::Platform::GLFW::Detail {
    CE::Input::KeyboardKey keyboard_key(const int key) {
        const auto* mapping = key >= 0 && key <= GLFW_KEY_LAST ? key_lookup[key] : nullptr;
        return mapping ? mapping->key : CE::Input::KeyboardKey::Unknown;
    }

    CE::Input::MouseButton mouse_button(const int button) {
        return button >= GLFW_MOUSE_BUTTON_1 && button <= GLFW_MOUSE_BUTTON_LAST
                   ? static_cast<CE::Input::MouseButton>(static_cast<int>(CE::Input::MouseButton::Left) + button - GLFW_MOUSE_BUTTON_1)
                   : CE::Input::MouseButton::Unknown;
    }

    CE::Input::ControlId gainput_keyboard_control(const int key) {
        const auto* mapping = key >= 0 && key <= GLFW_KEY_LAST ? key_lookup[key] : nullptr;
        return mapping ? mapping->gainput : gainput::InvalidDeviceButtonId;
    }

    CE::Input::ControlId gainput_mouse_control(const int button) {
        if (button < GLFW_MOUSE_BUTTON_1 || button > GLFW_MOUSE_BUTTON_LAST)
            return gainput::InvalidDeviceButtonId;
        if (button == GLFW_MOUSE_BUTTON_LEFT)
            return gainput::MouseButtonLeft;
        if (button == GLFW_MOUSE_BUTTON_RIGHT)
            return gainput::MouseButtonRight;
        if (button == GLFW_MOUSE_BUTTON_MIDDLE)
            return gainput::MouseButtonMiddle;
        return gainput::MouseButton5 + button - GLFW_MOUSE_BUTTON_4;
    }

    CE::Input::ControlId gamepad_control(const CE::Input::ControlId control) {
        return control < pad_lookup.size() ? pad_lookup[control] : CE::Input::invalid_control;
    }
}
