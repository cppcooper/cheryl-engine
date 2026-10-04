#pragma once

#include "poll-snapshot.h"
#include "tick-input.h"

#include <utility>

namespace CE::Input {
    /** Owns the last consumption boundary and persistent State baseline.
     * consume() summarizes a whole polling batch; it never schedules simulation.
     */
    class InputAccumulator final {
    public:
        InputAccumulator(std::shared_ptr<const ActionSnapshot> baseline, InputClock::time_point start)
        : previous_(std::move(baseline)), consumed_at_(start) {}

        [[nodiscard]] TickInput consume(InputClock::time_point until, std::vector<std::shared_ptr<const ActionSnapshot>> polls) {
            TickInput input(previous_, std::move(polls), consumed_at_, until);
            previous_ = input.latest_poll();
            consumed_at_ = until;
            return input;
        }

        [[nodiscard]] TickInput consume_polls(InputClock::time_point until, std::vector<std::shared_ptr<const PollSnapshot>> polls);

    private:
        std::shared_ptr<const ActionSnapshot> previous_;
        InputClock::time_point consumed_at_;
    };
}
