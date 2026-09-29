#pragma once

#include "action-snapshot.h"

#include <memory>
#include <span>
#include <vector>

namespace CE::Input {
    /** Button activity since the previous simulation consumption.
     * Durations use poll observation times, not unreported hardware times.
     */
    struct ButtonTickState {
        bool current = false;
        bool previous = false;
        bool pressed_this_tick = false;
        bool released_this_tick = false;
        std::uint64_t press_count = 0;
        std::uint64_t release_count = 0;
        InputDuration held_duration{}; // Age of the active hold, or zero when released.
        InputDuration down_duration{}; // Observed down-time in this consumption interval.
        std::vector<InputDuration> completed_holds; // Full duration of each hold released in this batch.

        [[nodiscard]] bool held() const { return current; }
        [[nodiscard]] bool pressed() const { return pressed_this_tick; }
        [[nodiscard]] bool released() const { return released_this_tick; }
    };

    /** Absolute axes retain their final value; relative axes sum this batch's deltas. */
    struct AxisTickState {
        float current = 0.0f;
        float previous = 0.0f;
        AxisKind kind = AxisKind::Absolute;

        [[nodiscard]] float delta() const { return kind == AxisKind::Relative ? current : current - previous; }
    };

    /** Immutable State view for one independently scheduled simulation update.
     * The baseline is the last consumed sample. New polls are consumed together
     * in publication order, regardless of how many transitions they contain.
     */
    class TickInput {
    public:
        // For manual consumers, the interval defaults to the sample timestamps.
        TickInput(std::shared_ptr<const ActionSnapshot> previous,
                  std::vector<std::shared_ptr<const ActionSnapshot>> polls);
        TickInput(std::shared_ptr<const ActionSnapshot> previous,
                  std::vector<std::shared_ptr<const ActionSnapshot>> polls,
                  InputClock::time_point since,
                  InputClock::time_point until);

        [[nodiscard]] ButtonTickState button(ActionId action) const;
        [[nodiscard]] AxisTickState axis(ActionId action) const;
        [[nodiscard]] std::span<const std::shared_ptr<const ActionSnapshot>> polls() const { return polls_; }
        [[nodiscard]] std::shared_ptr<const ActionSnapshot> latest_poll() const;
        [[nodiscard]] InputDuration elapsed() const { return until_ - since_; }

    private:
        std::shared_ptr<const ActionSnapshot> previous_;
        std::vector<std::shared_ptr<const ActionSnapshot>> polls_;
        InputClock::time_point since_;
        InputClock::time_point until_;
    };
}
