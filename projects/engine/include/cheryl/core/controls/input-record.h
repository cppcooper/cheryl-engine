#pragma once

#include "action-snapshot.h"
#include "button-codes.h"
#include "input-types.h"

#include <optional>
#include <variant>

namespace CE::Input {
    /** Portable source-device category; Other retains extension observations without claiming keyboard/mouse behavior. */
    enum class DeviceKind { Keyboard /**< Physical keyboard source. */, Mouse /**< Pointer/button/wheel source. */, Gamepad /**< Gamepad/controller source. */, Other /**< Adapter-specific device category. */ };
    namespace Events {
        /** Observed physical button transition; Repeat is not a new semantic press edge. */
    enum class ButtonPhase { Press /**< Newly observed button down. */, Release /**< Observed button up. */, Repeat /**< OS/backend repeat while down; not a new State press. */ };
    }

    /** Bitmask of observed Shift/Control/Alt/Super and lock-key state; None has no bits set. Combine named flags with operator|; other bits have no defined portable meaning. */
    enum class Modifiers : std::uint32_t { None = 0 /**< No modifier bits. */, Shift = 1 /**< Either Shift key. */, Control = 2 /**< Either Control key. */, Alt = 4 /**< Either Alt key. */, Super = 8 /**< Platform command/super key. */, CapsLock = 16 /**< Observed Caps Lock toggle. */, NumLock = 32 /**< Observed Num Lock toggle. */ };

    constexpr Modifiers operator|(const Modifiers a, const Modifiers b) {
        return static_cast<Modifiers>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
    }
    constexpr bool has_modifier(const Modifiers value, const Modifiers flag) {
        return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(flag)) != 0;
    }

    namespace Events {
        /** Logical window coordinates at observation time; typically origin top-left, independent of framebuffer scale. */
    struct Pointer {
            double x; ///< Logical horizontal coordinate.
            double y; ///< Logical vertical coordinate.
        }; // Logical window coordinates.

        /** Owned physical-button observation; portable and supplementary native identities are distinct. */
    struct Button {
            CE::Input::ControlId button; ///< Physical control token used for State mapping.
            ButtonPhase phase; ///< Press, release or repeat observation.
            CE::Input::Modifiers modifiers = CE::Input::Modifiers::None; ///< Modifier snapshot for this observation.
            // Backend codes supplement opaque control IDs, including an unmapped key.
            int native_code = -1; ///< Optional backend key/button token; -1 means unspecified.
            int scancode = -1; ///< Optional native keyboard scancode; -1 means unspecified.
            // Adapters identify known keyboard/mouse controls without exposing their
            // dependency's constants. Other or unrecognized controls stay Unknown.
            CE::Input::KeyboardKey key = CE::Input::KeyboardKey::Unknown; ///< Portable physical keyboard identity when known.
            CE::Input::MouseButton mouse_button = CE::Input::MouseButton::Unknown; ///< Portable mouse-button identity when known.
            // Observation-time position for mouse buttons, when the source provides
            // it. A later Pointer is not this click's position.
            std::optional<Pointer> position = std::nullopt; ///< Observed mouse position; a later cursor sample must not replace it.
        };
        /** One physical axis observation, separate from mapped semantic actions. */
    struct Axis {
            CE::Input::ControlId axis; ///< Physical axis control token.
            float value; ///< Observed value; interpretation/units belong to the physical control.
            CE::Input::AxisKind kind = CE::Input::AxisKind::Absolute; ///< Absolute retained value or relative delta.
        };
        /** Fractional wheel deltas, without conversion into synthetic button records. */
    struct Scroll {
            double x; ///< Horizontal fractional scroll offset.
            double y; ///< Vertical fractional scroll offset.
            std::optional<Pointer> position = std::nullopt; ///< Observation-time pointer position when supplied by the source.
        }; // Fractional scroll offsets, not synthetic buttons.
        /** One committed Unicode scalar from the OS; no IME composition or key-to-character synthesis. */
    struct Text {
            char32_t codepoint; ///< Valid scalar value; capture rejects surrogates and values above U+10FFFF.
        }; // A committed Unicode scalar from the OS.

        /** Owned physical observation or committed scalar, preserving their common record ordering. */
        using Payload = std::variant<Button, Axis, Pointer, Scroll, Text>;

        /** A backend-delivered observation. Sequence spans both Events and Text so
     * their relative order survives publication and the runtime thread handoff.
     * observed_at is callback/sample time, not a hardware event timestamp.
     * Sequence belongs to one collector, not a process-global hardware stream.
     * Payloads own their data; aggregate construction alone performs no validation.
     */
        struct Record {
            std::uint64_t sequence; ///< Collector-local sequence spanning Events and Text records.
            CE::Input::ObservationClock::time_point observed_at; ///< Callback/sample observation time, not a hardware timestamp.
            CE::Input::DeviceId device; ///< Adapter-owned source device token.
            CE::Input::DeviceKind device_kind; ///< Portable source category.
            Payload data; ///< Owned payload; aggregate construction itself performs no validation.
            CE::Input::Routing::TargetId target = 0; ///< Poll-latched keyboard/Text focus target;0 means no owner.
            std::uint64_t focus_epoch = 0; ///< Poll-latched focus request identity;0 means none.
            bool to_gameplay = true; ///< Gameplay admission; PassThrough permits focused keyboard records as well.

            [[nodiscard]] bool is_text() const { return std::holds_alternative<Text>(data); }
        };
    }
}

/** @fn constexpr CE::Input::Modifiers CE::Input::operator|(Modifiers a, Modifiers b)
 * Combine modifier bits without platform state access.
 */

/** @fn constexpr bool CE::Input::has_modifier(Modifiers value, Modifiers flag)
 * Return whether any bit in flag is set in value. None returns false; a combined flag tests any overlap, not all bits.
 */

/** @fn bool CE::Input::Events::Record::is_text() const
 * Return whether data holds committed Text, independently of device_kind.
 */
