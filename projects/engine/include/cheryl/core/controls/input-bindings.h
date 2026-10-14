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

namespace CE::Input::Actions {
    /** Controls held together in one poll, including ordinary modifier keys. Sequence detection belongs
     * to game logic; an unreported hardware combination cannot become an active chord.
     */
    struct Chord {
        std::vector<CE::Input::Control> required;
    };

    struct AxisOptions {
        float scale = 1.0f;     // A negative scale inverts the input.
        float dead_zone = 0.0f; // Finite [0, 1); inside becomes zero, the rest is rescaled without clamping.
        CE::Input::AxisKind kind = CE::Input::AxisKind::Absolute;
    };

    using BindingId = std::uint64_t;

    /** Owns physical-to-semantic mappings. Device notifications update pending physical state on the
     * polling thread; publish_actions() commits one immutable result for a platform poll. Configuration
     * and on_* calls belong to that same owner thread. Published handles may be read on other threads.
     */
    class Bindings {
        struct ButtonBinding {
            BindingId id;
            Chord chord;
            Id action;
        };
        struct AxisBinding {
            BindingId id;
            Chord modifiers;
            CE::Input::Control axis;
            Id action;
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
        std::unordered_set<CE::Input::Control> held_buttons_;
        std::unordered_set<CE::Input::DeviceId> disabled_devices_;
        std::unordered_map<CE::Input::Control, float> physical_axes_;
        std::unordered_map<Id, float> relative_actions_;
        std::unordered_map<Id, PendingButton> pending_buttons_;
        std::atomic<std::shared_ptr<const Snapshot>> published_{std::make_shared<Snapshot>()};

        [[nodiscard]] bool chord_active(const Chord& chord) const;
        [[nodiscard]] bool button_active(Id action) const;
        [[nodiscard]] std::unordered_map<Id, bool> evaluate_buttons() const;
        [[nodiscard]] std::unordered_map<Id, float> evaluate_axes() const;
        void refresh_button(Id action);

    public:
        /** Bind one physical control to a semantic button on the polling owner.
         * Multiple mappings to one button action combine with OR. Conflicting
         * button/axis use of an action is rejected.
         * @param control Physical device and button identity.
         * @param action Application-owned semantic identity read from tick input.
         * @return Nonzero registration ID for later unbind().
         */
        [[nodiscard]] BindingId bind_button(CE::Input::Control control, Id action);
        /** Bind an AND chord to a semantic button; alternative mappings combine with OR.
         * Empty chords and conflicting button/axis use of an action are rejected.
         * Configuration belongs to the polling owner.
         * @param chord Controls that must all be held in one poll.
         * @param action Application-owned semantic button identity.
         * @return Nonzero registration ID for later unbind().
         */
        [[nodiscard]] BindingId bind_button(Chord chord, Id action);
        // Several axis bindings contribute additively; a modifier chord gates its axis.
        // Scale/dead zone must be finite. Empty modifier chords are unconditional.
        [[nodiscard]] BindingId bind_axis(CE::Input::Control axis, Id action, AxisOptions options = {});
        [[nodiscard]] BindingId bind_axis(Chord modifiers, CE::Input::Control axis, Id action, AxisOptions options = {});
        /** Remove one mapping on the polling owner.
         * Removal affects the next publication; it does not erase already mapped
         * relative activity or earlier immutable handles.
         * @param binding Registration ID returned when the mapping was added.
         * @return Whether an existing mapping was removed; unknown IDs return false.
         */
        bool unbind(BindingId binding);
        void unbind_action(Id action);
        // Drop mappings/physical staging and publish release/zero State from the old baseline.
        void clear();
        // Platform-owned routing gate. Physical state is retained while semantic
        // actions are suppressed, so other devices and alternative mappings survive.
        void set_device_enabled(CE::Input::DeviceId device, bool enabled);

        // Backends report finite physical values; values are not clamped to [-1, 1].
        // Prior values are tracked here. Staging mutations are not transactional on failure.
        void on_axis(CE::Input::Control binding, float value);
        // Relative motion accumulates until publication; it does not persist into later polls.
        void on_delta(CE::Input::Control binding, float delta);
        void on_button(CE::Input::Control binding, bool held);

        // Call after one poll with nondecreasing observation time. Complete before
        // atomic publication; failure keeps the previous published handle. Success
        // clears pending transitions/relative activity. Poll IDs may skip on failure.
        [[nodiscard]] std::shared_ptr<const Snapshot>
        publish_actions(std::chrono::steady_clock::time_point observed_at = std::chrono::steady_clock::now());
        // Atomic retained-handle read from any thread, including before the first poll.
        [[nodiscard]] std::shared_ptr<const Snapshot> action_snapshot() const;
    };
} // namespace CE::Input::Actions
