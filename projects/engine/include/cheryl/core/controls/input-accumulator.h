#pragma once

#include "poll-snapshot.h"
#include "tick-input.h"

#include <utility>

namespace CE::Input {
    /** Owns the last consumption boundary and persistent State baseline.
     * consume() summarizes a whole polling batch; it never schedules simulation.
     * One owner supplies synchronization and a valid nonnull baseline/start.
     * Consumption validates sample/interval order before changing its baseline;
     * returned Tick owns handles and copied records independent of later calls.
     */
    class Accumulator final {
    public:
        Accumulator(std::shared_ptr<const Actions::Snapshot> baseline, ObservationClock::time_point start)
        : previous_(std::move(baseline)), consumed_at_(start) {}

        [[nodiscard]] Tick consume(ObservationClock::time_point until, std::vector<std::shared_ptr<const Actions::Snapshot>> polls) {
            Tick input(previous_, std::move(polls), consumed_at_, until);
            previous_ = input.latest_poll();
            consumed_at_ = until;
            return input;
        }

        [[nodiscard]] Tick consume_polls(ObservationClock::time_point until, std::vector<std::shared_ptr<const PollSnapshot>> polls);

    private:
        std::shared_ptr<const Actions::Snapshot> previous_;
        ObservationClock::time_point consumed_at_;
    };
}
