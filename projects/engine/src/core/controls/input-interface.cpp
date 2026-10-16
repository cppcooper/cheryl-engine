#include <core/controls/input-interface.h>

#include <internals/exceptions.h>

namespace CE::Input {
    void System::begin_input_poll() {
        // Apply the same focus snapshot to State gating and ordered routing.
        // Requests made during this pump take effect at the next poll.
        const auto focus = routing_.current();
        capture_.begin_poll(*focus);
        bindings().set_device_enabled(keyboard_id(), focus->target == 0 || focus->routing == Routing::KeyboardPolicy::PassThrough);
    }

    Capture::Lease System::capture(const Capture::Channel mode) {
        if (!supports(mode))
            throw Exceptions::failed_operation(CE_HERE, "This input adapter does not support the requested capture channel");
        return capture_.request(mode);
    }

    Routing::Manager& System::routing() {
        if (!supports_focus())
            throw Exceptions::failed_operation(CE_HERE, "This input adapter does not support keyboard focus routing");
        return routing_;
    }

    std::shared_ptr<const PollSnapshot> System::publish_input(const ObservationClock::time_point observed_at) {
        auto next = std::make_shared<PollSnapshot>();
        next->state = bindings().publish_actions(observed_at);
        next->records = capture_.complete();
        published_poll_.store(next, std::memory_order_release);
        return next;
    }

    std::shared_ptr<const PollSnapshot> System::poll_snapshot() {
        const auto state = action_snapshot();
        const auto complete = published_poll_.load(std::memory_order_acquire);
        if (complete && complete->state == state)
            return complete;
        if (supports(Capture::Channel::Events) || supports(Capture::Channel::Text))
            throw Exceptions::failed_operation(CE_HERE, "A capture-capable adapter must publish the complete input poll");
        // Existing State-only adapters may still call publish_actions(). Their
        // complete poll has no ordered records and cannot claim Events/Text support.
        return std::make_shared<PollSnapshot>(PollSnapshot{state, {}});
    }

    void System::discard_captured_input() {
        capture_.discard_pending();
        routing_.clear();
        published_poll_.store({}, std::memory_order_release);
    }
}

CE::Input::System::~System() = default;
