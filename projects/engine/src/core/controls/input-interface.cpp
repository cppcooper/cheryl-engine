#include <core/controls/input-interface.h>

#include <internals/exceptions.h>
#include <core/diagnostics.h>
#include <internals/failure-reporting.h>

#include <limits>
#include <utility>

namespace CE::Input {
    void System::begin_input_poll() {
        // Apply the same focus snapshot to State gating and ordered routing.
        // Requests made during this pump take effect at the next poll.
        const auto focus = routing_.current();
        capture_.begin_poll(*focus);
        bindings().set_device_enabled(keyboard_id(), focus->target == 0 || focus->routing == Routing::KeyboardPolicy::PassThrough);
        bindings().begin_poll();
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
        commit_gamepad_availability(*next);
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
        close_gamepad_availability();
    }

    void System::publish_events_to(CE::Events::EventBus::Publisher publisher) noexcept {
        gamepad_publisher_ = std::move(publisher);
    }

    GamepadAvailability System::gamepad_availability() const {
        std::lock_guard lock(gamepad_mutex_);
        return gamepad_;
    }

    void System::open_gamepad_availability() {
        if (publishing_gamepad_)
            throw Exceptions::failed_operation(CE_HERE, "Cannot reopen input during availability publication");
        const auto source = Diagnostics::next_domain_id();
        const auto slot = gamepad_.slot ? gamepad_.slot : Diagnostics::next_domain_id();
        if (!source || !slot)
            throw Exceptions::failed_operation(CE_HERE, "Gamepad observation identities exhausted");
        std::lock_guard lock(gamepad_mutex_);
        if (!gamepad_.closed)
            throw Exceptions::failed_operation(CE_HERE, "Gamepad observation source is already open");
        const auto epoch = gamepad_.epoch;
        gamepad_ = {};
        gamepad_.source = source;
        gamepad_.slot = slot;
        gamepad_.epoch = epoch;
        gamepad_.closed = false;
        published_gamepad_ = gamepad_;
        staged_gamepad_.reset();
    }

    void System::stage_gamepad_availability(const DeviceId device, const bool available) noexcept {
        staged_gamepad_ = std::pair{device, available};
    }

    void System::commit_gamepad_availability(const PollSnapshot& poll) noexcept {
        if (!staged_gamepad_)
            return; // Adapters without slot observation keep their existing poll path.
        try {
            if (staged_gamepad_) {
                const auto [device, available] = *std::exchange(staged_gamepad_, std::nullopt);
                std::lock_guard lock(gamepad_mutex_);
                if (!gamepad_.closed && (!gamepad_.known || gamepad_.available != available || gamepad_.device != device)) {
                    const bool new_epoch = available && (!gamepad_.known || !gamepad_.available || gamepad_.device != device);
                    // Keep one revision for closure. An exhausted observation source
                    // cannot turn a successfully completed input poll into failure.
                    if (gamepad_.revision >= std::numeric_limits<std::uint64_t>::max() - 1 ||
                        (new_epoch && gamepad_.epoch == std::numeric_limits<std::uint64_t>::max())) {
                        gamepad_.closed = true;
                        if (gamepad_.revision != std::numeric_limits<std::uint64_t>::max())
                            ++gamepad_.revision;
                        throw Exceptions::failed_operation(CE_HERE, "Gamepad observation sequence exhausted");
                    }
                    ++gamepad_.revision;
                    if (new_epoch)
                        ++gamepad_.epoch;
                    gamepad_.known = true;
                    gamepad_.available = available;
                    gamepad_.device = device;
                    gamepad_.poll = poll.state->poll();
                    gamepad_.observed_at = poll.state->observed_at();
                }
            }
        } catch (...) {
            Diagnostics::report_failure("gamepad availability commit", std::current_exception());
        }
        publish_gamepad_availability();
    }

    void System::publish_gamepad_availability() noexcept {
        if (publishing_gamepad_)
            return;
        publishing_gamepad_ = true;
        try {
            const auto take = [this]() -> std::optional<GamepadAvailabilityChanged> {
                std::lock_guard lock(gamepad_mutex_);
                if (gamepad_.source == published_gamepad_.source && gamepad_.revision == published_gamepad_.revision)
                    return std::nullopt;
                GamepadAvailabilityChanged result{gamepad_, published_gamepad_};
                published_gamepad_ = gamepad_;
                return result;
            };
            if (const auto change = take())
                gamepad_publisher_.notify(gamepad_availability_changed_event, *change);
            // Ordinary callback-created changes wait for the next poll. Explicit
            // closure may finish after the active fanout; reopening is disallowed
            // during this call, so this terminal flush cannot create a drain loop.
            if (gamepad_availability().closed) {
                if (const auto closed = take())
                    gamepad_publisher_.notify(gamepad_availability_changed_event, *closed);
            }
        } catch (...) {
            Diagnostics::report_failure("gamepad availability publication", std::current_exception());
        }
        publishing_gamepad_ = false;
    }

    void System::close_gamepad_availability(const bool notify) noexcept {
        try {
            std::lock_guard lock(gamepad_mutex_);
            if (gamepad_.closed)
                return;
            gamepad_.closed = true;
            if (gamepad_.revision != std::numeric_limits<std::uint64_t>::max())
                ++gamepad_.revision;
            gamepad_.observed_at = ObservationClock::now();
            staged_gamepad_.reset();
            if (!notify)
                published_gamepad_ = gamepad_;
        } catch (...) {
            Diagnostics::report_failure("gamepad availability closure", std::current_exception());
        }
        if (notify)
            publish_gamepad_availability();
    }
}

CE::Input::System::~System() = default;
