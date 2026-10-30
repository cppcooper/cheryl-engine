#pragma once

#include <cheryl/export.h>

#include "poll-snapshot.h"

#include <cstddef>
#include <memory>
#include <vector>

namespace CE::Input {
    /** Backpressure capacity between simulation consumptions; no policy drops completed polls. */
    enum class PollingPolicy { Lockstep /**< One poll per consumption. */, Finite /**< Up to configured capacity. */, Unlimited /**< No capacity bound. */ };

    /** Completed-poll capacity and completion-to-next-poll delay, validated by PollingBacklog. */
    struct PollingOptions {
        PollingPolicy policy = PollingPolicy::Lockstep; ///< Backpressure policy; Lockstep defaults to one completed observation.
        std::size_t capacity = 1; ///< Positive completed-poll limit for Finite, including unchanged observations.
        ObservationClock::duration spacing = std::chrono::milliseconds(1); ///< Nonnegative minimum delay from one completed poll to the next.
    };

    /** Platform-to-simulation polling batch. Its owner supplies synchronization.
     * Lockstep permits one completed poll per consumption; Finite permits capacity;
     * Unlimited has no capacity limit. Full batches pause polling, never drop data.
     */
    class PollingBacklog final {
    public:
        // Finite capacity must be positive; spacing nonnegative and policy known.
        CE_API explicit PollingBacklog(PollingOptions options = {});

        [[nodiscard]] CE_API bool can_poll() const;
        [[nodiscard]] CE_API bool poll_due(ObservationClock::time_point now) const;
        // max() when capacity is full; consumption restores capacity without resetting spacing.
        [[nodiscard]] CE_API ObservationClock::time_point next_poll_at() const;
        [[nodiscard]] std::size_t completed_polls() const { return polls_.size(); }
        // Requires eligible spacing/capacity, nonnull poll/State, a newer poll ID,
        // and observation time no later than completion. Invalid inputs throw;
        // allocation failure is not a rollback/retry guarantee for mutable staging.
        CE_API void complete(std::shared_ptr<const PollSnapshot> poll, ObservationClock::time_point completed_at);
        CE_API void complete(std::shared_ptr<const Actions::Snapshot> state, ObservationClock::time_point completed_at);
        // Transfers the whole batch and immediately leaves a fresh empty backlog.
        [[nodiscard]] CE_API std::vector<std::shared_ptr<const PollSnapshot>> consume();

    private:
        PollingOptions options_;
        ObservationClock::time_point next_poll_ = ObservationClock::time_point::min();
        std::uint64_t last_poll_ = 0;
        std::vector<std::shared_ptr<const PollSnapshot>> polls_;
    };
}

/** @fn CE::Input::PollingBacklog::PollingBacklog(PollingOptions options)
 * Create an empty owner-synchronized backlog. Unknown policy, negative spacing or zero Finite capacity throws invalid_args. First poll is immediately eligible.
 */

/** @fn bool CE::Input::PollingBacklog::can_poll() const
 * Return capacity eligibility only; poll_due() additionally checks minimum spacing.
 */

/** @fn bool CE::Input::PollingBacklog::poll_due(ObservationClock::time_point now) const
 * Return whether both capacity and completion spacing permit another poll at now.
 */

/** @fn CE::Input::ObservationClock::time_point CE::Input::PollingBacklog::next_poll_at() const
 * Return earliest spacing deadline, or time_point::max() while capacity is full. Consuming restores capacity but does not reset spacing.
 */

/** @fn std::size_t CE::Input::PollingBacklog::completed_polls() const
 * Return completed observations awaiting consumption, including unchanged polls.
 */

/** @fn void CE::Input::PollingBacklog::complete(std::shared_ptr<const PollSnapshot> poll, ObservationClock::time_point completed_at)
 * Retain a complete eligible poll. Require nonnull poll/state, strictly newer poll ID, observed_at<=completed_at, capacity and spacing. Invalid input throws; allocation failure does not promise rollback/retry of mutable staging. Advance the next deadline with saturation.
 */

/** @fn void CE::Input::PollingBacklog::complete(std::shared_ptr<const Actions::Snapshot> state, ObservationClock::time_point completed_at)
 * Wrap a State-only observation with empty records and apply complete-poll validation; allocation failures propagate.
 */

/** @fn auto CE::Input::PollingBacklog::consume() -> std::vector<std::shared_ptr<const PollSnapshot>>
 * Transfer the entire retained batch in publication order and leave fresh capacity immediately. Last poll ID/deadline remain for future validation/spacing.
 * @see [Polling backlog](cheryl-guide:runtime/input-state-model#polling-backlog-and-scheduling)
 */
