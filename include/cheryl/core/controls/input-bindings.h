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
        float scale = 1.0f; // A negative scale inverts the input.
        float dead_zone = 0.0f; // Values inside the zone become zero; the rest is rescaled.
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
        [[nodiscard]] BindingId bind_button(DeviceBind control, ActionId action);
        [[nodiscard]] BindingId bind_button(InputChord chord, ActionId action);
        // Several axis bindings contribute additively; a modifier chord gates its axis.
        [[nodiscard]] BindingId bind_axis(DeviceBind axis, ActionId action, AxisOptions options = {});
        [[nodiscard]] BindingId bind_axis(InputChord modifiers, DeviceBind axis, ActionId action, AxisOptions options = {});
        bool unbind(BindingId binding);
        void unbind_action(ActionId action);
        void clear();
        // Platform-owned routing gate. Physical state is retained while semantic
        // actions are suppressed, so other devices and alternative mappings survive.
        void set_device_enabled(DeviceId device, bool enabled);

        // Backends report current physical state; prior values are tracked here.
        void on_axis(DeviceBind binding, float value);
        // Relative motion accumulates until publication; it does not persist into later polls.
        void on_delta(DeviceBind binding, float delta);
        void on_button(DeviceBind binding, bool held);

        // Call after the backend finishes one poll. Each handle is a complete stable sample, not live input.
        [[nodiscard]] std::shared_ptr<const ActionSnapshot>
        publish_actions(std::chrono::steady_clock::time_point observed_at = std::chrono::steady_clock::now());
        [[nodiscard]] std::shared_ptr<const ActionSnapshot> action_snapshot() const;
    };
} // namespace CE::Input
