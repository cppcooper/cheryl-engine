#pragma once

#include "action-snapshot.h"
#include "device-binding.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace CE::Input {
    /** Controls held together in one poll, including ordinary modifier keys. Sequence detection belongs
     * to game logic; an unreported hardware combination cannot become an active chord.
     */
    struct InputChord {
        std::vector<DeviceBind> required;
    };

    struct AxisOptions {
        float scale = 1.0f;     // A negative scale inverts the input.
        float dead_zone = 0.0f; // Finite [0, 1); inside becomes zero, the rest is rescaled without clamping.
        AxisKind kind = AxisKind::Absolute;
    };

    using BindingId = std::uint64_t;

    /** Owns physical-to-semantic mappings. Device notifications update pending physical state on the
     * polling thread; publish_actions() commits one immutable result for a platform poll. Configuration
     * and on_* calls belong to that same owner thread. Published handles may be read on other threads.
     */
    class InputBindings {
        struct ButtonBinding {
            BindingId id;
            InputChord chord;
            ActionId action;
        };
        struct AxisBinding {
            BindingId id;
            InputChord modifiers;
            DeviceBind axis;
            ActionId action;
            AxisOptions options;
        };

        struct PendingButton {
            bool active = false;
            // Semantic transitions are retained until publication. State assigns
            // them the completed poll's observation time, including same-poll taps.
            std::vector<bool> transitions;
        };

        BindingId next_binding_ = 1;
        std::uint64_t next_poll_ = 1;
        std::vector<ButtonBinding> button_bindings_;
        std::vector<AxisBinding> axis_bindings_;
        std::unordered_set<DeviceBind> held_buttons_;
        std::unordered_set<DeviceId> disabled_devices_;
        std::unordered_map<DeviceBind, float> physical_axes_;
        std::unordered_map<ActionId, float> relative_actions_;
        std::unordered_map<ActionId, PendingButton> pending_buttons_;
        std::atomic<std::shared_ptr<const ActionSnapshot>> published_{std::make_shared<ActionSnapshot>()};

        [[nodiscard]] bool chord_active(const InputChord& chord) const;
        [[nodiscard]] bool button_active(ActionId action) const;
        [[nodiscard]] std::unordered_map<ActionId, bool> evaluate_buttons() const;
        [[nodiscard]] std::unordered_map<ActionId, float> evaluate_axes() const;
        void refresh_button(ActionId action);

    public:
        // Multiple mappings to one button action combine with OR; all controls in a chord use AND.
        // Empty button chords, button/axis conflicts and mixed axis kinds throw.
        [[nodiscard]] BindingId bind_button(DeviceBind control, ActionId action);
        [[nodiscard]] BindingId bind_button(InputChord chord, ActionId action);
        // Several axis bindings contribute additively; a modifier chord gates its axis.
        // Scale/dead zone must be finite. Empty modifier chords are unconditional.
        [[nodiscard]] BindingId bind_axis(DeviceBind axis, ActionId action, AxisOptions options = {});
        [[nodiscard]] BindingId bind_axis(InputChord modifiers, DeviceBind axis, ActionId action, AxisOptions options = {});
        // Unknown IDs return false. Removal affects the next publication; it does
        // not erase already mapped relative activity or earlier immutable handles.
        bool unbind(BindingId binding);
        void unbind_action(ActionId action);
        // Drop mappings/physical staging and publish release/zero State from the old baseline.
        void clear();
        // Platform-owned routing gate. Physical state is retained while semantic
        // actions are suppressed, so other devices and alternative mappings survive.
        void set_device_enabled(DeviceId device, bool enabled);

        // Backends report finite physical values; values are not clamped to [-1, 1].
        // Prior values are tracked here. Staging mutations are not transactional on failure.
        void on_axis(DeviceBind binding, float value);
        // Relative motion accumulates until publication; it does not persist into later polls.
        void on_delta(DeviceBind binding, float delta);
        void on_button(DeviceBind binding, bool held);

        // Call after one poll with nondecreasing observation time. Complete before
        // atomic publication; failure keeps the previous published handle. Success
        // clears pending transitions/relative activity. Poll IDs may skip on failure.
        [[nodiscard]] std::shared_ptr<const ActionSnapshot>
        publish_actions(std::chrono::steady_clock::time_point observed_at = std::chrono::steady_clock::now());
        // Atomic retained-handle read from any thread, including before the first poll.
        [[nodiscard]] std::shared_ptr<const ActionSnapshot> action_snapshot() const;
    };
} // namespace CE::Input
