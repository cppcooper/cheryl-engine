#pragma once

#include "input-types.h"

#include <cstdint>

namespace CE::Input {
    /** Portable physical key positions/functions, independent of native tokens.
     * Letter/punctuation names describe US keyboard positions, not typed text.
     * Named keys retain their ordinary keyboard meaning; Unknown is unmapped.
     * Committed characters come from Events::Text, including non-US layouts.
     * @see [Control identities](cheryl-guide:runtime/input-state-model#control-identities-and-binding-tokens)
     */
    enum class KeyboardKey : std::uint16_t {
        Unknown, ///< Unmapped or unsupported portable identity.
        A,
        B,
        C,
        D,
        E,
        F,
        G,
        H,
        I,
        J,
        K,
        L,
        M,
        N,
        O,
        P,
        Q,
        R,
        S,
        T,
        U,
        V,
        W,
        X,
        Y,
        Z,
        Digit0,
        Digit1,
        Digit2,
        Digit3,
        Digit4,
        Digit5,
        Digit6,
        Digit7,
        Digit8,
        Digit9,
        Space,
        Apostrophe,
        Comma,
        Minus,
        Period,
        Slash,
        Semicolon,
        Equal,
        LeftBracket,
        Backslash,
        RightBracket,
        GraveAccent,
        Escape,
        Enter,
        Tab,
        Backspace,
        Insert,
        Delete,
        Right,
        Left,
        Down,
        Up,
        PageUp,
        PageDown,
        Home,
        End,
        CapsLock,
        ScrollLock,
        NumLock,
        PrintScreen,
        Pause,
        F1,
        F2,
        F3,
        F4,
        F5,
        F6,
        F7,
        F8,
        F9,
        F10,
        F11,
        F12,
        F13,
        F14,
        F15,
        F16,
        F17,
        F18,
        F19,
        F20,
        F21,
        F22,
        F23,
        F24,
        F25,
        Keypad0,
        Keypad1,
        Keypad2,
        Keypad3,
        Keypad4,
        Keypad5,
        Keypad6,
        Keypad7,
        Keypad8,
        Keypad9,
        KeypadDecimal,
        KeypadDivide,
        KeypadMultiply,
        KeypadSubtract,
        KeypadAdd,
        KeypadEnter,
        KeypadEqual,
        LeftShift,
        LeftControl,
        LeftAlt,
        LeftSuper,
        RightShift,
        RightControl,
        RightAlt,
        RightSuper,
        Menu
    };

    /** Portable left/right/middle and five extra mouse buttons; Unknown is unmapped. */
    enum class MouseButton : std::uint8_t { Unknown /**< Unmapped button. */, Left /**< Primary left button. */, Right /**< Right button. */, Middle /**< Middle/wheel button. */, Extra1 /**< First adapter-mapped extra button. */, Extra2 /**< Second extra button. */, Extra3 /**< Third extra button. */, Extra4 /**< Fourth extra button. */, Extra5 /**< Fifth extra button. */ };

    /** Portable gamepad controls, reported only when supported by the device/adapter.
     * A/B/X/Y identify Xbox-style positions, not necessarily printed labels.
     * Axis6..31 and Button17..31 retain generic extension slots; sensor names
     * identify their axis and quantity without promising device availability.
     * Analog values are delivered without Engine clamping or unit conversion;
     * ranges/calibration and sensor units belong to the reporting adapter/device.
     * Button entries report held state; trigger axes and trigger buttons remain
     * separate controls. No universal sensor SI units or axis support are promised.
     * Unknown is unmapped.
     */
    enum class GamepadControl : std::uint16_t {
        Unknown, ///< Unmapped or unsupported portable identity.
        LeftStickX,
        LeftStickY,
        RightStickX,
        RightStickY,
        LeftTrigger,
        RightTrigger,
        Axis6,
        Axis7,
        Axis8,
        Axis9,
        Axis10,
        Axis11,
        Axis12,
        Axis13,
        Axis14,
        Axis15,
        Axis16,
        Axis17,
        Axis18,
        Axis19,
        Axis20,
        Axis21,
        Axis22,
        Axis23,
        Axis24,
        Axis25,
        Axis26,
        Axis27,
        Axis28,
        Axis29,
        Axis30,
        Axis31,
        AccelerationX,
        AccelerationY,
        AccelerationZ,
        GravityX,
        GravityY,
        GravityZ,
        GyroscopeX,
        GyroscopeY,
        GyroscopeZ,
        MagneticFieldX,
        MagneticFieldY,
        MagneticFieldZ,
        Start,
        Back,
        DPadLeft,
        DPadRight,
        DPadUp,
        DPadDown,
        A,
        B,
        X,
        Y,
        LeftShoulder,
        RightShoulder,
        LeftTriggerButton,
        RightTriggerButton,
        LeftStick,
        RightStick,
        Guide,
        Button17,
        Button18,
        Button19,
        Button20,
        Button21,
        Button22,
        Button23,
        Button24,
        Button25,
        Button26,
        Button27,
        Button28,
        Button29,
        Button30,
        Button31,
    };

    // Families have separate ID ranges. Adapters normalize observations to these
    // identities before binding evaluation; raw IDs remain available for extensions.
    [[nodiscard]] constexpr ControlId control_id(const KeyboardKey key) noexcept {
        return key > KeyboardKey::Unknown && key <= KeyboardKey::Menu ? 0x01000000u + static_cast<ControlId>(key) : invalid_control;
    }

    [[nodiscard]] constexpr ControlId control_id(const MouseButton button) noexcept {
        return button > MouseButton::Unknown && button <= MouseButton::Extra5 ? 0x02000000u + static_cast<ControlId>(button)
                                                                              : invalid_control;
    }

    [[nodiscard]] constexpr ControlId control_id(const GamepadControl control) noexcept {
        return control > GamepadControl::Unknown && control <= GamepadControl::Button31 ? 0x03000000u + static_cast<ControlId>(control)
                                                                                        : invalid_control;
    }
}

/** @fn constexpr CE::Input::ControlId CE::Input::control_id(KeyboardKey key) noexcept
 * Map a known portable key into the keyboard control range. Unknown and values beyond Menu return invalid_control; no native SDK token or committed character is produced.
 */

/** @fn constexpr CE::Input::ControlId CE::Input::control_id(MouseButton button) noexcept
 * Map a known mouse button into a range distinct from keys/gamepads. Unknown and out-of-range values return invalid_control.
 */

/** @fn constexpr CE::Input::ControlId CE::Input::control_id(GamepadControl control) noexcept
 * Map a known named/generic gamepad control into its portable family range. Unknown/out-of-range returns invalid_control; availability is still adapter/device-dependent.
 */
