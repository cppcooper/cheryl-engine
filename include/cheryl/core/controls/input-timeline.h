#pragma once

#include "tick-input.h"
#include <internals/exceptions.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <span>
#include <utility>

namespace CE::Input {
    /** Advances simulation through the input states observed since its previous update.
     * A poll's timestamp is when its state became observable, not a hardware event time.
     * The caller can publish a render frame after all intervals have advanced.
     */
    class InputTimeline final {
    public:
        using Clock = std::chrono::steady_clock;

        InputTimeline(std::shared_ptr<const ActionSnapshot> baseline, Clock::time_point start) :
            previous_(std::move(baseline)), at_(start) {}

        template <typename Update>
        void advance(Clock::time_point until, std::span<const std::shared_ptr<const ActionSnapshot>> changes,
                     Update&& update) {
            // A clock regression cannot apply negative simulation time.
            until = std::max(until, at_);
            auto cursor = at_;
            auto previous = previous_;
            std::shared_ptr<const ActionSnapshot> pending;

            const auto emit = [&](const Clock::time_point end) {
                // An edge belongs to the interval after it was observed. Even a zero-length
                // interval delivers a tap before a second event at the same timestamp.
                auto input = pending ? TickInput::from_poll(previous, pending)
                                     : TickInput(previous, {});
                update(std::chrono::duration<double>(end - cursor).count(), input);
                if (pending) previous = std::exchange(pending, {});
                cursor = end;
            };

            for (const auto& poll : changes) {
                if (!poll)
                    throw Exceptions::invalid_args(CE_HERE, "Input timeline requires a completed poll");
                const auto observed = std::clamp(poll->observed_at(), cursor, until);
                if (observed > cursor || pending) emit(observed);
                pending = poll;
            }
            emit(until);
            previous_ = std::move(previous);
            at_ = until;
        }

    private:
        std::shared_ptr<const ActionSnapshot> previous_;
        Clock::time_point at_;
    };
}
