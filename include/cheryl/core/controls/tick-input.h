#pragma once

#include "action-snapshot.h"

#include <memory>
#include <vector>

namespace CE::Input {
    /** Button state across all input polls consumed by one simulation tick. */
    struct ButtonTickState {
        bool current = false;
        bool previous = false;
        bool pressed_this_tick = false;
        bool released_this_tick = false;

        [[nodiscard]] bool held() const { return current; }
        [[nodiscard]] bool pressed() const { return pressed_this_tick; }
        [[nodiscard]] bool released() const { return released_this_tick; }
    };

    /** The first and last axis values consumed by one simulation tick. */
    struct AxisTickState {
        float current = 0.0f;
        float previous = 0.0f;

        [[nodiscard]] float delta() const { return current - previous; }
    };

    /** Read-only input for one simulation tick, built from zero or more completed polls.
     * previous is the last poll consumed by the preceding tick; polls are in publication order.
     * Retaining every poll allows game logic to inspect transitions in their observed order.
     */
    class TickInput {
    public:
        TickInput(std::shared_ptr<const ActionSnapshot> previous,
                  std::vector<std::shared_ptr<const ActionSnapshot>> polls);

        [[nodiscard]] ButtonTickState button(ActionId action) const;
        [[nodiscard]] AxisTickState axis(ActionId action) const;
        [[nodiscard]] const std::vector<std::shared_ptr<const ActionSnapshot>>& polls() const { return polls_; }
        [[nodiscard]] std::shared_ptr<const ActionSnapshot> latest_poll() const;

    private:
        std::shared_ptr<const ActionSnapshot> previous_;
        std::vector<std::shared_ptr<const ActionSnapshot>> polls_;
    };
} // namespace CE::Input
