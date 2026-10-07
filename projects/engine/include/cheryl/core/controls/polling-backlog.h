#pragma once

#include "poll-snapshot.h"

#include <cstddef>
#include <memory>
#include <vector>

namespace CE::Input {
    enum class PollingPolicy { Lockstep, Finite, Unlimited };

    struct PollingOptions {
        PollingPolicy policy = PollingPolicy::Lockstep;
        std::size_t capacity = 1;                                    // Completed polls, including unchanged observations; used by Finite.
        InputClock::duration spacing = std::chrono::milliseconds(1); // Minimum delay after completing a poll.
    };

    /** Platform-to-simulation polling batch. Its owner supplies synchronization.
     * Lockstep permits one completed poll per consumption; Finite permits capacity;
     * Unlimited has no capacity limit. Full batches pause polling, never drop data.
     */
    class PollingBacklog final {
    public:
        // Finite capacity must be positive; spacing nonnegative and policy known.
        explicit PollingBacklog(PollingOptions options = {});

        [[nodiscard]] bool can_poll() const;
        [[nodiscard]] bool poll_due(InputClock::time_point now) const;
        // max() when capacity is full; consumption restores capacity without resetting spacing.
        [[nodiscard]] InputClock::time_point next_poll_at() const;
        [[nodiscard]] std::size_t completed_polls() const { return polls_.size(); }
        // Requires eligible spacing/capacity, nonnull poll/State, a newer poll ID,
        // and observation time no later than completion. Invalid inputs throw;
        // allocation failure is not a rollback/retry guarantee for mutable staging.
        void complete(std::shared_ptr<const PollSnapshot> poll, InputClock::time_point completed_at);
        void complete(std::shared_ptr<const ActionSnapshot> state, InputClock::time_point completed_at);
        // Transfers the whole batch and immediately leaves a fresh empty backlog.
        [[nodiscard]] std::vector<std::shared_ptr<const PollSnapshot>> consume();

    private:
        PollingOptions options_;
        InputClock::time_point next_poll_ = InputClock::time_point::min();
        std::uint64_t last_poll_ = 0;
        std::vector<std::shared_ptr<const PollSnapshot>> polls_;
    };
}
