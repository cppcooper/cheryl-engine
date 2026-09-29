#include <core/controls/polling-backlog.h>

#include <internals/exceptions.h>

#include <utility>

namespace CE::Input {
    PollingBacklog::PollingBacklog(const PollingOptions options) : options_(options) {
        if (options.spacing < InputClock::duration::zero())
            throw Exceptions::invalid_args(CE_HERE, "Polling spacing cannot be negative");
        switch (options.policy) {
            case PollingPolicy::Lockstep:
            case PollingPolicy::Unlimited:
                break;
            case PollingPolicy::Finite:
                if (options.capacity == 0)
                    throw Exceptions::invalid_args(CE_HERE, "Finite polling requires a positive completed-poll capacity");
                break;
            default:
                throw Exceptions::invalid_args(CE_HERE, "Unknown input polling policy");
        }
    }

    bool PollingBacklog::can_poll() const {
        return options_.policy == PollingPolicy::Unlimited ||
            polls_.size() < (options_.policy == PollingPolicy::Lockstep ? 1 : options_.capacity);
    }

    bool PollingBacklog::poll_due(const InputClock::time_point now) const { return can_poll() && now >= next_poll_; }

    InputClock::time_point PollingBacklog::next_poll_at() const { return can_poll() ? next_poll_ : InputClock::time_point::max(); }

    void PollingBacklog::complete(std::shared_ptr<const ActionSnapshot> poll, const InputClock::time_point completed_at) {
        if (!poll_due(completed_at) || !poll || poll->poll() <= last_poll_ || poll->observed_at() > completed_at)
            throw Exceptions::invalid_args(CE_HERE, "Polling backlog requires an eligible, newer completed observation");
        last_poll_ = poll->poll();
        polls_.push_back(std::move(poll));
        // Handoff resets capacity, not this delay: two polls on either side of
        // consumption still respect the completion-to-next-poll spacing.
        const auto remaining = InputClock::time_point::max() - completed_at;
        next_poll_ = options_.spacing < remaining ? completed_at + options_.spacing : InputClock::time_point::max();
    }

    std::vector<std::shared_ptr<const ActionSnapshot>> PollingBacklog::consume() { return std::exchange(polls_, {}); }
}
