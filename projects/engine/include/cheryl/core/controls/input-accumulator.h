#pragma once

#include <cheryl/export.h>

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

        [[nodiscard]] CE_API Tick consume_polls(ObservationClock::time_point until, std::vector<std::shared_ptr<const PollSnapshot>> polls);

    private:
        std::shared_ptr<const Actions::Snapshot> previous_;
        ObservationClock::time_point consumed_at_;
    };
}

/** @fn CE::Input::Accumulator::Accumulator(std::shared_ptr<const Actions::Snapshot> baseline, ObservationClock::time_point start)
 * Retain a baseline and initial consumption boundary on one synchronized owner. The constructor stores inputs; consume operations validate nonnull baseline and monotonic intervals.
 */

/** @fn CE::Input::Tick CE::Input::Accumulator::consume(ObservationClock::time_point until, std::vector<std::shared_ptr<const Actions::Snapshot>> polls)
 * Summarize State-only polls into an owned Tick over the previous boundary..until interval. Validate newer IDs/nondecreasing observation times before advancing the baseline/boundary; failure preserves them. Empty polls preserve current levels while hold durations advance.
 */

/** @fn CE::Input::Tick CE::Input::Accumulator::consume_polls(ObservationClock::time_point until, std::vector<std::shared_ptr<const PollSnapshot>> polls)
 * Summarize complete polls and copy their records in publication order without consuming another reader's stream. Null polls/state, invalid order/intervals and allocation failures throw before changing the baseline. Return independent Tick storage and retained samples.
 * @see [Tick input](cheryl-guide:runtime/input-state-model#tick-input)
 */
