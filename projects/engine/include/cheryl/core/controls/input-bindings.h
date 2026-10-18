#pragma once

#include <cheryl/export.h>

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
    class ContextSubmission;
    namespace Detail {
        struct ContextsState;
    }
    /** Controls held together in one poll, including ordinary modifier keys. Sequence detection belongs
     * to game logic; an unreported hardware combination cannot become an active chord.
     */
    struct Chord {
        std::vector<CE::Input::Control> required; ///< AND controls held in one observed poll; button chords must be nonempty.
    };

    /** Per-binding linear/dead-zone mapping; contributions to an action add without clamping. */
    struct AxisOptions {
        float scale = 1.0f; ///< Finite multiplier; negative values invert the contribution.
        float dead_zone = 0.0f; ///< Finite[0,1); inside becomes zero, remaining magnitude is rescaled without clamping.
        CE::Input::AxisKind kind = CE::Input::AxisKind::Absolute; ///< Absolute sample or per-poll relative delta; one action cannot mix kinds.
    };

    /** Nonzero registration token returned by one Bindings owner; separate from semantic Id and context namespace. */
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
        std::shared_ptr<Detail::ContextsState> contexts_;
        bool poll_started_ = false;

        [[nodiscard]] CE_API bool chord_active(const Chord& chord) const;
        [[nodiscard]] CE_API bool button_active(Id action) const;
        [[nodiscard]] CE_API std::unordered_map<Id, bool> evaluate_buttons() const;
        [[nodiscard]] CE_API std::unordered_map<Id, float> evaluate_axes() const;
        CE_API void refresh_button(Id action);
        [[nodiscard]] CE_API std::shared_ptr<Snapshot> make_snapshot(std::chrono::steady_clock::time_point observed_at);
        CE_API void commit_snapshot(std::shared_ptr<const Snapshot> snapshot) noexcept;
        CE_API void clear_contexts();

    public:
        Bindings() = default;
        CE_API ~Bindings();
        // Acquire on this mapper's owner; the returned endpoint is thread-safe.
        [[nodiscard]] CE_API ContextSubmission action_contexts();
        // Optional explicit poll boundary. on_* and publication start a boundary
        // lazily for existing State-only adapters. Requests arriving mid-poll wait.
        CE_API void begin_poll();
        [[nodiscard]] CE_API BindingId bind_button(CE::Input::Control control, Id action);
        [[nodiscard]] CE_API BindingId bind_button(Chord chord, Id action);
        // Several axis bindings contribute additively; a modifier chord gates its axis.
        // Scale/dead zone must be finite. Empty modifier chords are unconditional.
        [[nodiscard]] CE_API BindingId bind_axis(CE::Input::Control axis, Id action, AxisOptions options = {});
        [[nodiscard]] CE_API BindingId bind_axis(Chord modifiers, CE::Input::Control axis, Id action, AxisOptions options = {});
        CE_API bool unbind(BindingId binding);
        CE_API void unbind_action(Id action);
        // Drop mappings/physical staging and publish release/zero State from the old baseline.
        CE_API void clear();
        // Platform-owned routing gate. Physical state is retained while semantic
        // actions are suppressed, so other devices and alternative mappings survive.
        CE_API void set_device_enabled(CE::Input::DeviceId device, bool enabled);

        // Backends report finite physical values; values are not clamped to [-1, 1].
        // Prior values are tracked here. Staging mutations are not transactional on failure.
        CE_API void on_axis(CE::Input::Control binding, float value);
        // Relative motion accumulates until publication; it does not persist into later polls.
        CE_API void on_delta(CE::Input::Control binding, float delta);
        CE_API void on_button(CE::Input::Control binding, bool held);

        // Call after one poll with nondecreasing observation time. Complete before
        // atomic publication; failure keeps the previous published handle. Success
        // clears pending transitions/relative activity. Poll IDs may skip on failure.
        [[nodiscard]] CE_API std::shared_ptr<const Snapshot>
        publish_actions(std::chrono::steady_clock::time_point observed_at = std::chrono::steady_clock::now());
        // Atomic retained-handle read from any thread, including before the first poll.
        [[nodiscard]] CE_API std::shared_ptr<const Snapshot> action_snapshot() const;
    };
} // namespace CE::Input::Actions

/** @fn CE::Input::Actions::Bindings::Bindings()
 * Create an empty mapper with a retained initial snapshot, no mappings and lazy context-endpoint state. Construction/allocation may fail.
 */

/** @fn CE::Input::Actions::Bindings::~Bindings()
 * Close retained context endpoints and invalidate leases after mapper users quiesce. Published immutable snapshots and retained context identities remain usable.
 */

/** @fn CE::Input::Actions::ContextSubmission CE::Input::Actions::Bindings::action_contexts()
 * Acquire on the polling/binding owner; retain the returned synchronized endpoint to attach definitions from other producers. It does not retain this mapper or adapter.
 */

/** @fn void CE::Input::Actions::Bindings::begin_poll()
 * Apply accepted context additions/removals at an explicit poll boundary, seeding held buttons/current absolute axes/device gates. Mid-poll requests wait for the next boundary; prior relative deltas are not replayed. Legacy on_* or publication opens a boundary lazily. Evaluation/allocation failures may propagate.
 */

/** @fn CE::Input::Actions::BindingId CE::Input::Actions::Bindings::bind_button(CE::Input::Control control, Id action)
 * Bind one known physical control on the polling owner; equivalent to a one-control chord. Return a nonzero registration token. Multiple button mappings to one action combine with OR; button/axis conflicts and pending relative activity reject.
 */

/** @fn CE::Input::Actions::BindingId CE::Input::Actions::Bindings::bind_button(Chord chord, Id action)
 * Own a nonempty AND chord; alternatives for the same action combine with OR. Reject invalid controls, axis conflicts or pending relative activity before registration. Allocation can fail; binding IDs may skip. Device IDs are values, not borrowed device objects.
 * @see [Bindings](cheryl-guide:runtime/input-state-model#bindings)
 */

/** @fn CE::Input::Actions::BindingId CE::Input::Actions::Bindings::bind_axis(CE::Input::Control axis, Id action, AxisOptions options)
 * Bind an unconditional axis contribution; equivalent to an empty modifier chord. Return its nonzero registration token. Scale/dead-zone/kind and action compatibility are validated.
 */

/** @fn CE::Input::Actions::BindingId CE::Input::Actions::Bindings::bind_axis(Chord modifiers, CE::Input::Control axis, Id action, AxisOptions options)
 * Own a modifier-gated additive axis contribution; empty modifiers are unconditional. Reject unknown controls, nonfinite scale/dead-zone, dead-zone outside[0,1), invalid/mixed kinds or button conflicts. Pending relative activity must be published before changing to absolute/button behavior; allocation failures propagate.
 */

/** @fn bool CE::Input::Actions::Bindings::unbind(BindingId binding)
 * Remove one mapping on the polling owner, returning false for unknown tokens. Next publication reflects removal but preserves already mapped relative activity and old snapshots.
 */

/** @fn void CE::Input::Actions::Bindings::unbind_action(Id action)
 * Remove all direct mappings to this semantic action, without modifying independent context namespaces or retained snapshots. Already mapped relative activity survives until publication.
 */

/** @fn void CE::Input::Actions::Bindings::clear()
 * Invalidate context leases and drop mappings/physical staging/device gates, then publish release/zero state from the old baseline. Retained endpoints remain open for later attachment. Publication/allocation can throw after mutable state is cleared; this is not atomic rollback.
 */

/** @fn void CE::Input::Actions::Bindings::set_device_enabled(CE::Input::DeviceId device, bool enabled)
 * Gate semantic contributions while retaining physical state, including active contexts. Re-enabling can restore held controls/absolute values; other devices and alternative mappings remain independent. Use on the platform polling owner; staging failures need not roll back.
 */

/** @fn void CE::Input::Actions::Bindings::on_axis(CE::Input::Control binding, float value)
 * Stage a finite absolute physical value without clamping[-1,1]. Updates pending semantic transitions as needed; opens a lazy poll boundary. Invalid values/allocation/scaled overflow can throw after partial staging.
 */

/** @fn void CE::Input::Actions::Bindings::on_delta(CE::Input::Control binding, float delta)
 * Accumulate finite relative motion using mappings/device gates active when delivered. Unbinding later does not erase admitted motion; successful publication resets it. Nonfinite/scaled/summed overflow and allocation failures throw without transactional staging guarantees.
 */

/** @fn void CE::Input::Actions::Bindings::on_button(CE::Input::Control binding, bool held)
 * Stage physical held state and preserve semantic press/release transitions, including same-poll taps. Repeated identical levels do not create an edge. Opens a lazy poll boundary; allocation failures may leave partial staging.
 */

/** @fn std::shared_ptr<const CE::Input::Actions::Snapshot> CE::Input::Actions::Bindings::publish_actions(std::chrono::steady_clock::time_point observed_at)
 * Complete and atomically publish one immutable direct/context snapshot on the polling owner. Require nondecreasing observation time. Failure preserves the preceding published handle and pending transitions for retry; poll IDs can skip. Success clears transitions/relative activity and retains one release/zero sample for removed actions.
 * @return Owned retained state readable from other threads.
 */

/** @fn std::shared_ptr<const CE::Input::Actions::Snapshot> CE::Input::Actions::Bindings::action_snapshot() const
 * Atomically retain the latest immutable handle from any thread, including the initial empty snapshot before the first publication. Does not poll devices.
 */
