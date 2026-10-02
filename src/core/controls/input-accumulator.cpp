#include <core/controls/input-accumulator.h>

#include <internals/exceptions.h>

namespace CE::Input {
    TickInput InputAccumulator::consume_polls(const InputClock::time_point until, std::vector<std::shared_ptr<const PollSnapshot>> polls) {
        std::vector<std::shared_ptr<const ActionSnapshot>> states;
        states.reserve(polls.size());
        std::size_t record_count = 0;
        for (const auto& poll : polls) {
            if (!poll || !poll->state)
                throw Exceptions::invalid_args(CE_HERE, "Input consumption requires a complete poll");
            states.push_back(poll->state);
            record_count += poll->records.size();
        }
        TickInput input(previous_, std::move(states), consumed_at_, until);
        input.records_.reserve(record_count);
        // Concatenate the independently captured stream in poll publication order.
        // State summarization never removes, repeats, or reorders these records.
        for (const auto& poll : polls)
            input.records_.insert(input.records_.end(), poll->records.begin(), poll->records.end());
        previous_ = input.latest_poll();
        consumed_at_ = until;
        return input;
    }
}
