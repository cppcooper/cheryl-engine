#pragma once

#include "input-types.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace CE::Input {
    /** Monotonic clock for poll/callback observation, independent of simulation timing. */
    using ObservationClock = std::chrono::steady_clock;
    /** Floating observation-clock seconds; not scaled simulation time. */
    using ObservationDuration = std::chrono::duration<double>;
    /** Axis accumulation policy. */
    enum class AxisKind { Absolute /**< Retain final sampled value. */, Relative /**< Sum deltas within the poll/tick. */ };

} // namespace CE::Input

namespace CE::Input::Actions {
    /** Runtime context-namespace identity; zero means no lease, not a persistent asset ID. */
    using ContextId = std::uint64_t;
    /** Game-defined semantic identifier. A game's enums can be converted explicitly at its boundary. */
    struct Id {
        std::uint32_t value; ///< Application semantic value; all uint32 values, including zero, are available.
        constexpr explicit Id(std::uint32_t id)
        : value(id) {}
        constexpr bool operator==(const Id&) const = default;
    };
} // namespace CE::Input::Actions

template <> struct std::hash<CE::Input::Actions::Id> {
    std::size_t operator()(const CE::Input::Actions::Id id) const noexcept { return std::hash<std::uint32_t>{}(id.value); }
};

namespace CE::Input::Actions {
    /** State at one completed input poll; both edges may be true for a press and release in that poll. */
    struct ButtonPollState {
        bool current = false; ///< Final held state at this publication.
        bool previous = false; ///< Held state from the preceding publication.
        bool pressed_this_poll = false; ///< At least one semantic press in this poll.
        bool released_this_poll = false; ///< At least one semantic release in this poll.
        std::uint64_t press_count = 0; ///< Number of retained press transitions in this poll.
        std::uint64_t release_count = 0; ///< Number of retained release transitions in this poll.
        std::optional<CE::Input::ObservationClock::time_point> hold_started_at; ///< Observed start of an active hold; absent when released.
        std::vector<CE::Input::ObservationDuration> completed_holds; ///< Owned full durations of holds released during this poll, in observation seconds.

        [[nodiscard]] bool held() const { return current; }
        [[nodiscard]] bool pressed() const { return pressed_this_poll; }
        [[nodiscard]] bool released() const { return released_this_poll; }
    };

    /** One completed axis observation; kind determines whether current is retained position or this poll's relative sum. */
    struct AxisPollState {
        float current = 0.0f; ///< Final absolute value or accumulated relative delta.
        float previous = 0.0f; ///< Preceding absolute value; relative delta baseline is zero.
        CE::Input::AxisKind kind = CE::Input::AxisKind::Absolute; ///< Absolute retention or per-poll relative accumulation.

        [[nodiscard]] float delta() const { return kind == CE::Input::AxisKind::Relative ? current : current - previous; }
    };

    /** The complete set of semantic actions from one poll. Copies/handles stay stable after later polls. */
    class Snapshot {
        friend class Bindings;
        std::uint64_t poll_ = 0;
        std::chrono::steady_clock::time_point observed_at_ = std::chrono::steady_clock::now();
        std::unordered_map<Id, ButtonPollState> buttons_;
        std::unordered_map<Id, AxisPollState> axes_;
        std::unordered_map<ContextId, std::shared_ptr<const Snapshot>> contexts_;
        std::unordered_set<CE::Input::DeviceId> disabled_devices_;

    public:
        [[nodiscard]] std::uint64_t poll() const { return poll_; }
        [[nodiscard]] std::chrono::steady_clock::time_point observed_at() const { return observed_at_; }
        [[nodiscard]] bool has_context(ContextId context) const noexcept { return contexts_.contains(context); }
        [[nodiscard]] bool device_enabled(CE::Input::DeviceId device) const noexcept { return !disabled_devices_.contains(device); }
        [[nodiscard]] bool has_changes() const {
            for (const auto& [id, state] : buttons_)
                if (state.pressed() || state.released())
                    return true;
            for (const auto& [id, state] : axes_)
                if (state.delta() != 0.0f)
                    return true;
            for (const auto& [id, context] : contexts_)
                if (context->has_changes())
                    return true;
            return false;
        }
        // Owned state copies; missing actions return inactive/zero defaults.
        [[nodiscard]] ButtonPollState button(Id action) const {
            const auto found = buttons_.find(action);
            return found == buttons_.end() ? ButtonPollState{} : found->second;
        }
        [[nodiscard]] AxisPollState axis(Id action) const {
            const auto found = axes_.find(action);
            return found == axes_.end() ? AxisPollState{} : found->second;
        }
        [[nodiscard]] bool has_axis(Id action) const { return axes_.contains(action); }
        [[nodiscard]] ButtonPollState button(ContextId context, Id action) const {
            const auto found = contexts_.find(context);
            return found == contexts_.end() ? ButtonPollState{} : found->second->button(action);
        }
        [[nodiscard]] AxisPollState axis(ContextId context, Id action) const {
            const auto found = contexts_.find(context);
            return found == contexts_.end() ? AxisPollState{} : found->second->axis(action);
        }
        [[nodiscard]] bool has_axis(ContextId context, Id action) const {
            const auto found = contexts_.find(context);
            return found != contexts_.end() && found->second->has_axis(action);
        }
    };
} // namespace CE::Input::Actions

/** @fn constexpr CE::Input::Actions::Id::Id(std::uint32_t id)
 * Explicitly wrap a game semantic value without assigning global ownership or reserving subranges.
 */

/** @fn constexpr bool CE::Input::Actions::Id::operator==(const Id&) const
 * Compare the semantic numeric values; context namespace is supplied separately.
 */

/** @struct std::hash<CE::Input::Actions::Id>
 * Hash semantic Id by its numeric value; not a namespace-qualified or persistent hash.
 */

/** @fn std::size_t std::hash<CE::Input::Actions::Id>::operator()(CE::Input::Actions::Id id) const noexcept
 * Hash the underlying uint32 value without accessing input state.
 */

/** @fn bool CE::Input::Actions::ButtonPollState::held() const
 * Return final held state; both edges can still be true after a same-poll tap.
 */

/** @fn bool CE::Input::Actions::ButtonPollState::pressed() const
 * Return whether one or more press transitions occurred, not only the final level.
 */

/** @fn bool CE::Input::Actions::ButtonPollState::released() const
 * Return whether one or more release transitions occurred, independently of press presence.
 */

/** @fn float CE::Input::Actions::AxisPollState::delta() const
 * For Relative return current; for Absolute return current-previous. Units follow the mapped axis and its configured scale.
 */

/** @fn std::uint64_t CE::Input::Actions::Snapshot::poll() const
 * Return publication ID, initially0; monotonic publications may skip IDs on failed preparation.
 */

/** @fn std::chrono::steady_clock::time_point CE::Input::Actions::Snapshot::observed_at() const
 * Return completed poll observation time, not simulation or hardware time.
 */

/** @fn bool CE::Input::Actions::Snapshot::has_changes() const
 * Report any button edge/axis delta in direct or contextual state. Context membership/device admission alone does not count as a change; an unchanged poll still represents a completed observation.
 */

/** @fn bool CE::Input::Actions::Snapshot::has_context(ContextId context) const noexcept
 * Test namespace presence in this immutable sample, including a departing context's final release/zero sample. Zero, pending/not-yet-published and subsequently retired identities are absent. Retention preserves history, not a live lease; callers requiring a live candidate must also retain and validate its lease and generation. Read-only queries may cross threads.
 */

/** @fn bool CE::Input::Actions::Snapshot::device_enabled(CE::Input::DeviceId device) const noexcept
 * Return this sample's publication-time admission policy. Unlisted devices are enabled, including before the first poll; this does not establish hardware existence. Later mapper/focus changes never rewrite retained samples. Read-only queries may cross threads.
 */

/** @fn CE::Input::Actions::ButtonPollState CE::Input::Actions::Snapshot::button(Id action) const
 * Return an owned direct-namespace button state; an absent action returns inactive defaults. Copying completed-hold storage can allocate.
 */

/** @fn CE::Input::Actions::AxisPollState CE::Input::Actions::Snapshot::axis(Id action) const
 * Return a direct-namespace axis copy; absent actions return zero Absolute state.
 */

/** @fn bool CE::Input::Actions::Snapshot::has_axis(Id action) const
 * Test whether a direct axis is explicitly present, distinguishing absent state from a reported zero.
 */

/** @fn CE::Input::Actions::ButtonPollState CE::Input::Actions::Snapshot::button(ContextId context, Id action) const
 * Return a context-local button copy. Unknown/absent context or action returns inactive defaults; retained snapshots are not rewritten by lease removal.
 */

/** @fn CE::Input::Actions::AxisPollState CE::Input::Actions::Snapshot::axis(ContextId context, Id action) const
 * Return context-local axis state; missing context/action returns zero Absolute defaults.
 */

/** @fn bool CE::Input::Actions::Snapshot::has_axis(ContextId context, Id action) const
 * Test explicit axis presence within the given retained namespace; no live lease is required.
 */
