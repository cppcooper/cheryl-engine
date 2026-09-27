#pragma once

#include "action-snapshot.h"
#include "device-binding.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
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
    };

    using BindingId = std::uint64_t;

    /** Owns physical-to-semantic mappings. Device notifications update pending physical state on the
     * polling thread; publish_actions() commits one immutable result for a simulation tick. Configuration
     * and on_* calls belong to that same owner thread. Published handles may be read on other threads.
     */
    class InputBindings {
    public:
        // Legacy callbacks are kept for adapters/games that still use them. They execute on the poll thread.
        void bind_axis(DeviceBind binding, std::function<void(float, float)> callback);
        void bind_button(DeviceBind binding, std::function<void(bool, bool)> callback);

        // Multiple mappings to one button action combine with OR; all controls in a chord use AND.
        [[nodiscard]] BindingId bind_button(DeviceBind control, ActionId action);
        [[nodiscard]] BindingId bind_button(InputChord chord, ActionId action);
        // Several axis bindings contribute additively; a modifier chord gates its axis.
        [[nodiscard]] BindingId bind_axis(DeviceBind axis, ActionId action, AxisOptions options = {});
        [[nodiscard]] BindingId bind_axis(InputChord modifiers, DeviceBind axis, ActionId action, AxisOptions options = {});
        bool unbind(BindingId binding);
        void unbind_action(ActionId action);
        void clear();

        void on_axis(DeviceBind binding, float old_value, float new_value);
        void on_button(DeviceBind binding, bool old_value, bool new_value);

        // Call after the backend finishes one poll. Each handle is a complete stable sample, not live input.
        [[nodiscard]] std::shared_ptr<const ActionSnapshot> publish_actions();
        [[nodiscard]] std::shared_ptr<const ActionSnapshot> action_snapshot() const;

    private:
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

        [[nodiscard]] bool chord_active(const InputChord& chord) const;
        [[nodiscard]] std::unordered_map<ActionId, bool> evaluate_buttons() const;
        [[nodiscard]] std::unordered_map<ActionId, float> evaluate_axes() const;

        BindingId next_binding_ = 1;
        std::uint64_t next_poll_ = 1;
        std::vector<ButtonBinding> button_bindings_;
        std::vector<AxisBinding> axis_bindings_;
        std::unordered_map<DeviceBind, bool> physical_buttons_;
        std::unordered_map<DeviceBind, float> physical_axes_;
        std::unordered_map<ActionId, bool> last_button_active_;
        std::unordered_map<ActionId, bool> pending_presses_;
        std::unordered_map<ActionId, bool> pending_releases_;
        std::atomic<std::shared_ptr<const ActionSnapshot>> published_{std::make_shared<ActionSnapshot>()};
        std::unordered_map<DeviceBind, std::function<void(float, float)>> axis_callbacks_;
        std::unordered_map<DeviceBind, std::function<void(bool, bool)>> button_callbacks_;
    };
} // namespace CE::Input
