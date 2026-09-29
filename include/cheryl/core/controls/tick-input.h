#pragma once

#include "action-snapshot.h"

#include <memory>
#include <span>
#include <utility>
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

    /** Read-only input for one simulation update. The previous state is the last
     * consumed poll; optional new polls are in publication order. Runtime updates
     * contain at most one changed poll, while callers can still inspect a batch.
     */
    class TickInput {
    public:
        TickInput(std::shared_ptr<const ActionSnapshot> previous,
                  std::vector<std::shared_ptr<const ActionSnapshot>> polls);
        // The runtime's common single-poll interval keeps only a handle, without a vector allocation.
        [[nodiscard]] static TickInput from_poll(std::shared_ptr<const ActionSnapshot> previous,
                                                 std::shared_ptr<const ActionSnapshot> poll);

        [[nodiscard]] ButtonTickState button(ActionId action) const;
        [[nodiscard]] AxisTickState axis(ActionId action) const;
        [[nodiscard]] std::span<const std::shared_ptr<const ActionSnapshot>> polls() const;
        [[nodiscard]] std::shared_ptr<const ActionSnapshot> latest_poll() const;

    private:
        TickInput(std::shared_ptr<const ActionSnapshot> previous, std::shared_ptr<const ActionSnapshot> poll,
                  std::in_place_t);
        std::shared_ptr<const ActionSnapshot> previous_;
        std::vector<std::shared_ptr<const ActionSnapshot>> polls_;
        std::shared_ptr<const ActionSnapshot> single_poll_;
    };
} // namespace CE::Input
