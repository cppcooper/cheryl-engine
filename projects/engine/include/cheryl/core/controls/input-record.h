#pragma once

#include "action-snapshot.h"
#include "button-codes.h"
#include "input-types.h"

#include <optional>
#include <variant>

namespace CE::Input {
    enum class DeviceKind { Keyboard, Mouse, Gamepad, Other };
    enum class ButtonPhase { Press, Release, Repeat };
    enum class Modifiers : std::uint32_t { None = 0, Shift = 1, Control = 2, Alt = 4, Super = 8, CapsLock = 16, NumLock = 32 };

    constexpr Modifiers operator|(const Modifiers a, const Modifiers b) {
        return static_cast<Modifiers>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
    }
    constexpr bool has_modifier(const Modifiers value, const Modifiers flag) {
        return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(flag)) != 0;
    }

    struct PointerEvent {
        double x;
        double y;
    }; // Logical window coordinates.

    struct ButtonEvent {
        DeviceButtonId button;
        ButtonPhase phase;
        Modifiers modifiers = Modifiers::None;
        // Backend codes supplement opaque control IDs, including an unmapped key.
        int native_code = -1;
        int scancode = -1;
        // Adapters identify known keyboard/mouse controls without exposing their
        // dependency's constants. Other or unrecognized controls stay Unknown.
        KeyboardKey key = KeyboardKey::Unknown;
        MouseButton mouse_button = MouseButton::Unknown;
        // Observation-time position for mouse buttons, when the source provides
        // it. A later PointerEvent is not this click's position.
        std::optional<PointerEvent> position = std::nullopt;
    };
    struct AxisEvent {
        DeviceButtonId axis;
        float value;
        AxisKind kind = AxisKind::Absolute;
    };
    struct ScrollEvent {
        double x;
        double y;
        std::optional<PointerEvent> position = std::nullopt;
    }; // Fractional scroll offsets, not synthetic buttons.
    struct TextEvent {
        char32_t codepoint;
    }; // A committed Unicode scalar from the OS.

    using InputRecordData = std::variant<ButtonEvent, AxisEvent, PointerEvent, ScrollEvent, TextEvent>;

    /** A backend-delivered observation. Sequence spans both Events and Text so
     * their relative order survives publication and the runtime thread handoff.
     * observed_at is callback/sample time, not a hardware event timestamp.
     */
    struct InputRecord {
        std::uint64_t sequence;
        InputClock::time_point observed_at;
        DeviceId device;
        DeviceKind device_kind;
        InputRecordData data;
        FocusId target = 0;
        std::uint64_t focus_epoch = 0;
        bool to_gameplay = true; // PassThrough can also deliver a focused keyboard event to gameplay.

        [[nodiscard]] bool is_text() const { return std::holds_alternative<TextEvent>(data); }
    };
}
