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

    TickInput::TickInput(std::shared_ptr<const ActionSnapshot> previous,
                         std::shared_ptr<const ActionSnapshot> poll, std::in_place_t) :
        previous_(std::move(previous)), single_poll_(std::move(poll)) {
        if (!previous_ || !single_poll_ || single_poll_->poll() <= previous_->poll())
            throw Exceptions::invalid_args(CE_HERE, "TickInput requires a newer completed poll");
    }

    TickInput TickInput::from_poll(std::shared_ptr<const ActionSnapshot> previous,
                                   std::shared_ptr<const ActionSnapshot> poll) {
        return TickInput(std::move(previous), std::move(poll), std::in_place);
    }

    std::span<const std::shared_ptr<const ActionSnapshot>> TickInput::polls() const {
        return single_poll_ ? std::span{&single_poll_, std::size_t{1}} : std::span{polls_};
    }

    ButtonTickState TickInput::button(const ActionId action) const {
        const bool previous = previous_->button(action).held();
        ButtonTickState result{previous, previous};
        for (const auto& poll : polls()) {
            const auto state = poll->button(action);
            result.current = state.held();
            result.pressed_this_tick |= state.pressed();
            result.released_this_tick |= state.released();
        }
        return result;
    }

    AxisTickState TickInput::axis(const ActionId action) const {
        const float previous = previous_->axis(action).current;
        const auto completed = polls();
        const float current = completed.empty() ? previous : completed.back()->axis(action).current;
        return {current, previous};
    }

    std::shared_ptr<const ActionSnapshot> TickInput::latest_poll() const {
        const auto completed = polls();
        return completed.empty() ? previous_ : completed.back();
    }
} // namespace CE::Input
