#pragma once

#include "action-snapshot.h"
#include "button-codes.h"
#include "input-types.h"

#include <optional>
#include <variant>

namespace CE::Input {
    enum class DeviceKind { Keyboard, Mouse, Gamepad, Other };
    namespace Events {
        enum class ButtonPhase { Press, Release, Repeat };
    }

    enum class Modifiers : std::uint32_t { None = 0, Shift = 1, Control = 2, Alt = 4, Super = 8, CapsLock = 16, NumLock = 32 };

    constexpr Modifiers operator|(const Modifiers a, const Modifiers b) {
        return static_cast<Modifiers>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
    }
    constexpr bool has_modifier(const Modifiers value, const Modifiers flag) {
        return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(flag)) != 0;
    }

    namespace Events {
        struct Pointer {
            double x;
            double y;
        }; // Logical window coordinates.

        struct Button {
            CE::Input::ControlId button;
            ButtonPhase phase;
            CE::Input::Modifiers modifiers = CE::Input::Modifiers::None;
            // Backend codes supplement opaque control IDs, including an unmapped key.
            int native_code = -1;
            int scancode = -1;
            // Adapters identify known keyboard/mouse controls without exposing their
            // dependency's constants. Other or unrecognized controls stay Unknown.
            CE::Input::KeyboardKey key = CE::Input::KeyboardKey::Unknown;
            CE::Input::MouseButton mouse_button = CE::Input::MouseButton::Unknown;
            // Observation-time position for mouse buttons, when the source provides
            // it. A later Pointer is not this click's position.
            std::optional<Pointer> position = std::nullopt;
        };
        struct Axis {
            CE::Input::ControlId axis;
            float value;
            CE::Input::AxisKind kind = CE::Input::AxisKind::Absolute;
        };
        struct Scroll {
            double x;
            double y;
            std::optional<Pointer> position = std::nullopt;
        }; // Fractional scroll offsets, not synthetic buttons.
        struct Text {
            char32_t codepoint;
        }; // A committed Unicode scalar from the OS.

        using Payload = std::variant<Button, Axis, Pointer, Scroll, Text>;

        /** A backend-delivered observation. Sequence spans both Events and Text so
     * their relative order survives publication and the runtime thread handoff.
     * observed_at is callback/sample time, not a hardware event timestamp.
     * Sequence belongs to one collector, not a process-global hardware stream.
     * Payloads own their data; aggregate construction alone performs no validation.
     */
        struct Record {
            std::uint64_t sequence;
            CE::Input::ObservationClock::time_point observed_at;
            CE::Input::DeviceId device;
            CE::Input::DeviceKind device_kind;
            Payload data;
            CE::Input::Routing::TargetId target = 0;
            std::uint64_t focus_epoch = 0;
            bool to_gameplay = true; // PassThrough can also deliver a focused keyboard event to gameplay.

            [[nodiscard]] bool is_text() const { return std::holds_alternative<Text>(data); }
        };
    }
}
