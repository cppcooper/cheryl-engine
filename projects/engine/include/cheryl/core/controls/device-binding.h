#pragma once

#include "button-codes.h"

#include <functional>
#include <type_traits>

namespace CE::Input {
    /** Physical (device, control) identity for bindings. Initialize both fields before using a default-initialized Control; no device lifetime is retained. Typed constructors accept only supported control enums; unrelated enums are deleted. */
    struct Control {
        DeviceId device; ///< Adapter-owned device token.
        ControlId control; ///< Portable or adapter extension control token; invalid_control is rejected by binding APIs.

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

/** @fn constexpr CE::Input::Control::Control()
 * Default construction supplies no explicit member values; initialize device/control before querying or binding.
 */

/** @fn constexpr CE::Input::Control::Control(DeviceId device, ControlId control) noexcept
 * Store raw tokens without validation. Binding evaluation later validates control identities; this value does not own a device.
 */

/** @fn constexpr CE::Input::Control::Control(DeviceId device, KeyboardKey key) noexcept
 * Store device with control_id(key); Unknown becomes invalid_control for later rejection.
 */

/** @fn constexpr CE::Input::Control::Control(DeviceId device, MouseButton button) noexcept
 * Store device with control_id(button), independently of native mouse SDK tokens.
 */

/** @fn constexpr CE::Input::Control::Control(DeviceId device, GamepadControl gamepad) noexcept
 * Store device with control_id(gamepad); the named control may still be unavailable on that device.
 */

/** @fn bool CE::Input::Control::operator==(const Control& o) const
 * Compare both physical tokens by value; neither device availability nor current held state is queried.
 */

/** @struct std::hash<CE::Input::Control>
 * Hash the device/control pair for physical-state maps. Equal Control values have equal hashes; this is not a persistence identifier.
 */

/** @fn std::size_t std::hash<CE::Input::Control>::operator()(const CE::Input::Control& k) const noexcept
 * Combine both initialized physical tokens without querying a live device.
 */
