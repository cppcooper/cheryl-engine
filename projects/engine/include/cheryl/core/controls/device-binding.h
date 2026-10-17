#pragma once

#include "button-codes.h"

#include <functional>
#include <type_traits>

namespace CE::Input {
    struct Control {
        DeviceId device;
        ControlId control;

        constexpr Control() = default;
        constexpr Control(const DeviceId device, const ControlId control) noexcept
        : device(device), control(control) {}
        constexpr Control(const DeviceId device, const KeyboardKey key) noexcept
        : Control(device, control_id(key)) {}
        constexpr Control(const DeviceId device, const MouseButton button) noexcept
        : Control(device, control_id(button)) {}
        constexpr Control(const DeviceId device, const GamepadControl gamepad) noexcept
        : Control(device, control_id(gamepad)) {}

        template <typename Enum>
            requires std::is_enum_v<Enum>
        Control(DeviceId, Enum) = delete;

        bool operator==(const Control& o) const { return device == o.device && control == o.control; }
    };
}

namespace std {
    template <> struct hash<CE::Input::Control> {
        std::size_t operator()(const CE::Input::Control& k) const noexcept {
            std::hash<CE::Input::ControlId> hash_button;
            std::hash<CE::Input::DeviceId> hash_device;
            const std::size_t hash1 = hash_button(k.control);
            return hash1 ^ (hash_device(k.device) + 0x9e3779b9 + (hash1 << 6) + (hash1 >> 2));
        }
    };
}
