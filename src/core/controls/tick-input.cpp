#include <core/controls/tick-input.h>

#include <internals/exceptions.h>

#include <utility>

namespace CE::Input {
    TickInput::TickInput(std::shared_ptr<const ActionSnapshot> previous,
                         std::vector<std::shared_ptr<const ActionSnapshot>> polls) :
        previous_(std::move(previous)), polls_(std::move(polls)) {
        if (!previous_)
            throw Exceptions::invalid_args(CE_HERE, "TickInput requires the previously consumed poll");

        auto last_poll = previous_->poll();
        for (const auto& poll : polls_) {
            if (!poll || poll->poll() <= last_poll)
                throw Exceptions::invalid_args(CE_HERE, "TickInput requires newer polls in publication order");
            last_poll = poll->poll();
        }
    }

    ButtonTickState TickInput::button(const ActionId action) const {
        const bool previous = previous_->button(action).held();
        ButtonTickState result{previous, previous};
        for (const auto& poll : polls_) {
            const auto state = poll->button(action);
            result.current = state.held();
            result.pressed_this_tick |= state.pressed();
            result.released_this_tick |= state.released();
        }
        return result;
    }

    AxisTickState TickInput::axis(const ActionId action) const {
        const float previous = previous_->axis(action).current;
        const float current = polls_.empty() ? previous : polls_.back()->axis(action).current;
        return {current, previous};
    }

    std::shared_ptr<const ActionSnapshot> TickInput::latest_poll() const {
        return polls_.empty() ? previous_ : polls_.back();
    }
} // namespace CE::Input
