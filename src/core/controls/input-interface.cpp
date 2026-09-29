#include <core/controls/input-interface.h>

#include <internals/exceptions.h>

namespace CE::Input {
    CaptureLease iInputSystem::capture(const InputMode mode) {
        if (!supports(mode))
            throw Exceptions::failed_operation(CE_HERE, "This input adapter does not support the requested capture channel");
        return capture_.request(mode);
    }

    std::shared_ptr<const PollSnapshot> iInputSystem::publish_input(const InputClock::time_point observed_at) {
        auto next = std::make_shared<PollSnapshot>();
        next->state = bindings().publish_actions(observed_at);
        next->records = capture_.complete();
        published_poll_.store(next, std::memory_order_release);
        return next;
    }

    std::shared_ptr<const PollSnapshot> iInputSystem::poll_snapshot() {
        const auto state = action_snapshot();
        const auto complete = published_poll_.load(std::memory_order_acquire);
        if (complete && complete->state == state)
            return complete;
        // Existing State-only adapters may still call publish_actions(). Their
        // complete poll has no ordered records and cannot claim Events/Text support.
        return std::make_shared<PollSnapshot>(PollSnapshot{state, {}});
    }

    void iInputSystem::discard_captured_input() {
        capture_.discard_pending();
        published_poll_.store({}, std::memory_order_release);
    }
}
