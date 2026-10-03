#include <core/controls/tick-input.h>

#include <internals/exceptions.h>

#include <algorithm>
#include <utility>

namespace CE::Input {
    TickInput::TickInput(std::shared_ptr<const ActionSnapshot> previous, std::vector<std::shared_ptr<const ActionSnapshot>> polls)
    : TickInput(
          previous,
          polls,
          previous ? previous->observed_at() : InputClock::time_point{},
          !polls.empty() && polls.back() ? polls.back()->observed_at()
          : previous                     ? previous->observed_at()
                                         : InputClock::time_point{}
      ) {}

    TickInput::TickInput(
        std::shared_ptr<const ActionSnapshot> previous,
        std::vector<std::shared_ptr<const ActionSnapshot>> polls,
        const InputClock::time_point since,
        const InputClock::time_point until
    )
    : previous_(std::move(previous)), polls_(std::move(polls)), since_(since), until_(until) {
        if (!previous_ || since < previous_->observed_at() || until < since)
            throw Exceptions::invalid_args(CE_HERE, "TickInput requires a baseline and a monotonic consumption interval");
        auto last_poll = previous_->poll();
        auto last_time = previous_->observed_at();
        for (const auto& poll : polls_) {
            if (!poll || poll->poll() <= last_poll || poll->observed_at() < last_time || poll->observed_at() > until)
                throw Exceptions::invalid_args(CE_HERE, "TickInput requires newer completed polls in observation order");
            last_poll = poll->poll();
            last_time = poll->observed_at();
        }
    }

    ButtonTickState TickInput::button(const ActionId action) const {
        const auto baseline = previous_->button(action);
        ButtonTickState result;
        result.current = baseline.current;
        result.previous = baseline.current;
        auto cursor = since_;
        for (const auto& poll : polls_) {
            const auto observed = std::max(cursor, poll->observed_at());
            if (result.current)
                result.down_duration += observed - cursor;
            const auto state = poll->button(action);
            result.current = state.current;
            result.press_count += state.press_count;
            result.release_count += state.release_count;
            result.completed_holds.insert(result.completed_holds.end(), state.completed_holds.begin(), state.completed_holds.end());
            cursor = observed;
        }
        if (result.current) {
            result.down_duration += until_ - cursor;
            const auto last = latest_poll()->button(action);
            if (last.hold_started_at)
                result.held_duration = until_ - *last.hold_started_at;
        }
        result.pressed_this_tick = result.press_count != 0;
        result.released_this_tick = result.release_count != 0;
        return result;
    }

    AxisTickState TickInput::axis(const ActionId action) const {
        const auto baseline = previous_->axis(action);
        const auto latest = latest_poll()->axis(action);
        // An unbound relative action can disappear from later samples. Keep
        // the last explicitly reported kind rather than treating absence as Absolute.
        std::optional<AxisKind> kind;
        for (const auto& poll : polls_) {
            if (!poll->has_axis(action))
                continue;
            const auto observed = poll->axis(action).kind;
            if (kind && *kind != observed)
                throw Exceptions::invalid_args(CE_HERE, "Changing an axis kind requires an input consumption boundary");
            kind = observed;
        }
        if (kind.value_or(baseline.kind) == AxisKind::Relative) {
            float delta = 0.0f;
            for (const auto& poll : polls_)
                if (const auto state = poll->axis(action); state.kind == AxisKind::Relative)
                    delta += state.current;
            return {delta, 0.0f, AxisKind::Relative};
        }
        return {latest.current, baseline.kind == AxisKind::Absolute ? baseline.current : 0.0f, AxisKind::Absolute};
    }

    std::shared_ptr<const ActionSnapshot> TickInput::latest_poll() const {
        return polls_.empty() ? previous_ : polls_.back();
    }
}
