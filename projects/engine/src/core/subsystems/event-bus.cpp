#include <core/subsystems/event-bus.h>
#include <internals/exceptions.h>
#include <internals/compile-time-logging.hpp>

#include <limits>
#include <algorithm>
#include <future>
#include <utility>

namespace {
    // A linked stack also detects a wait from a nested invocation of the same
    // listener, without allocating memory after its running count increments.
    struct Invocation {
        const void* listener;
        Invocation* previous;
    };
    thread_local Invocation* current_invocation = nullptr;
}

namespace CE::SubSystems {
    EventBus::~EventBus() {
        close();
    }

    EventBus::Registration
    EventBus::register_listener(const std::string& event, Callback callback, Delivery delivery, ErrorHandler errors) {
        return register_channel(event, typeid(void), std::move(callback), std::move(delivery), std::move(errors));
    }

    EventBus::Registration
    EventBus::register_channel(
        const std::string& event,
        const std::type_index payload,
        Callback callback,
        Delivery delivery,
        ErrorHandler errors
    ) {
        if (!callback)
            throw Exceptions::invalid_args(CE_HERE, "An event listener requires a callback");
        if (delivery && !errors)
            throw Exceptions::invalid_args(CE_HERE, "Queued event delivery requires an error handler");
        auto listener = std::make_shared<Listener>();
        listener->counters = state_->counters;
        listener->event = {event, payload};
        listener->callback = std::move(callback);
        listener->delivery = std::move(delivery);
        listener->errors = std::move(errors);
        if (listener->delivery)
            listener->cancellation = std::make_exception_ptr(std::future_error(std::future_errc::broken_promise));
        std::lock_guard lock(state_->mutex);
        if (state_->closed)
            throw Exceptions::failed_operation(CE_HERE, "EventBus is closed");
        if (state_->next_id == std::numeric_limits<std::uint64_t>::max())
            throw Exceptions::failed_operation(CE_HERE, "Event registration IDs exhausted");
        const auto id = state_->next_id;
        state_->channels[listener->event].push_back(listener);
        ++state_->next_id;
        ++state_->registrations;
        ++state_->active;
        return Registration(state_, listener, id);
    }

    void EventBus::dispatch(const std::string& event, const std::any& payload) {
        dispatch_channel(event, typeid(void), payload);
    }

    void EventBus::dispatch_channel(const std::string_view event, const std::type_index type, const std::any& payload) {
        std::vector<std::shared_ptr<Listener>> snapshot;
        {
            std::lock_guard lock(state_->mutex);
            if (state_->closed)
                throw Exceptions::failed_operation(CE_HERE, "EventBus is closed");
            ++state_->dispatches;
            const auto found = state_->channels.find(ChannelView{event, type});
            if (found == state_->channels.end())
                return;
            snapshot = found->second;
        }
        // Registry edits cannot invalidate this dispatch; no registry lock is
        // held while arbitrary callbacks register more listeners or dispatch.
        for (const auto& listener : snapshot)
            deliver(listener, payload);
    }

    EventBus::DeliveryTicket::DeliveryTicket(std::shared_ptr<Listener> value)
    : listener(std::move(value)), failure(listener->cancellation) {}

    EventBus::DeliveryTicket::~DeliveryTicket() {
        if (entered.load(std::memory_order_acquire))
            return;
        bool active;
        {
            std::lock_guard lock(listener->mutex);
            active = listener->active;
        }
        // Dropped target work must not silently lose an event. Explicit
        // unregister/close intentionally discard delivery without reporting.
        if (active)
            report_error(listener, failure);
        else
            listener->counters->discarded.fetch_add(1, std::memory_order_relaxed);
    }

    void EventBus::report_error(const std::shared_ptr<Listener>& listener, std::exception_ptr failure) noexcept {
        listener->counters->queued_failures.fetch_add(1, std::memory_order_relaxed);
        // Error sinks must not throw. Termination makes a broken sink visible
        // instead of hiding it in a discarded dispatch-target future.
        listener->errors(std::move(failure));
    }

    void EventBus::deliver(const std::shared_ptr<Listener>& listener, const std::any& payload) {
        if (!listener->delivery) {
            invoke(listener, payload);
            return;
        }
        {
            std::lock_guard lock(listener->mutex);
            if (!listener->active) {
                listener->counters->discarded.fetch_add(1, std::memory_order_relaxed);
                return;
            }
        }
        std::shared_ptr<DeliveryTicket> ticket;
        try {
            // Preparation belongs to queued delivery's error contract as well.
            // Keep the entire ticket/payload local through posting. A target may
            // destroy rejected work while the posting lock is still held; neither
            // its error sink nor a payload destructor may reenter under that lock.
            ticket = std::make_shared<DeliveryTicket>(listener);
            ticket->payload = payload;
            Work work([ticket] {
                ticket->entered.store(true, std::memory_order_release);
                try {
                    invoke(ticket->listener, ticket->payload);
                } catch (...) {
                    report_error(ticket->listener, std::current_exception());
                }
            });
            // Concurrent producers linearize target enqueue for this listener.
            // The target defers execution; no callback is invoked under this lock.
            std::lock_guard lock(listener->posting);
            (void)listener->delivery(std::move(work));
        } catch (...) {
            if (ticket)
                ticket->failure = std::current_exception();
            else {
                // Ticket allocation itself failed. No accepted work exists to
                // report later, and no registry/entry/posting lock is held here.
                bool active;
                {
                    std::lock_guard lock(listener->mutex);
                    active = listener->active;
                }
                if (active)
                    report_error(listener, std::current_exception());
            }
        }
        // The local owner releases rejection reporting and the copied payload
        // outside posting, including payload destructors which dispatch again.
    }

    void EventBus::invoke(const std::shared_ptr<Listener>& listener, const std::any& payload) {
        {
            std::lock_guard lock(listener->mutex);
            // Check and enter are one transaction with unregister. A snapshot
            // alone must not authorize a callback after invalidation returns.
            if (!listener->active) {
                listener->counters->discarded.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            ++listener->running;
            listener->counters->invocations.fetch_add(1, std::memory_order_relaxed);
        }
        struct InvocationGuard {
            std::shared_ptr<Listener> listener;
            Invocation invocation;

            explicit InvocationGuard(const std::shared_ptr<Listener>& value)
            : listener(value), invocation{value.get(), current_invocation} {
                current_invocation = &invocation;
            }
            ~InvocationGuard() {
                current_invocation = invocation.previous;
                std::lock_guard lock(listener->mutex);
                if (--listener->running == 0)
                    listener->idle.notify_all();
            }
        } guard(listener);
        listener->callback(payload);
    }

    void EventBus::invalidate(const std::shared_ptr<Listener>& listener) {
        std::lock_guard lock(listener->mutex);
        listener->active = false;
    }

    bool EventBus::unregister_listener(const Registration& registration) {
        if (registration.bus_.lock() != state_)
            return false;
        auto listener = registration.listener_.lock();
        if (!listener)
            return false;
        bool removed = false;
        {
            std::lock_guard lock(state_->mutex);
            // Publish invalidation before detaching. A concurrent close must
            // never miss a removed listener whose invocation gate is still open.
            invalidate(listener);
            const auto channel = state_->channels.find(listener->event);
            if (channel != state_->channels.end()) {
                removed = std::erase(channel->second, listener) != 0;
                if (removed)
                    --state_->active;
                if (channel->second.empty())
                    state_->channels.erase(channel);
            }
        }
        // The local owner releases callback captures after the registry unlocks.
        return removed;
    }

    void EventBus::wait_for_listener(const Registration& registration) const {
        if (registration.bus_.lock() != state_)
            throw Exceptions::invalid_args(CE_HERE, "Registration belongs to another event bus");
        auto listener = registration.listener_.lock();
        if (!listener)
            return;
        for (auto* invocation = current_invocation; invocation; invocation = invocation->previous)
            if (invocation->listener == listener.get())
                throw Exceptions::failed_operation(CE_HERE, "An event listener cannot wait for its own invocation");
        std::unique_lock lock(listener->mutex);
        if (listener->active)
            throw Exceptions::failed_operation(CE_HERE, "Unregister the listener before waiting for completion");
        listener->idle.wait(lock, [&] { return listener->running == 0; });
    }

    bool EventBus::unregister_and_wait(const Registration& registration) {
        const bool removed = unregister_listener(registration);
        wait_for_listener(registration);
        return removed;
    }

    void EventBus::close() {
        decltype(State::channels) detached;
        {
            std::lock_guard lock(state_->mutex);
            state_->closed = true;
            state_->active = 0;
            // Registry -> listener is the lock order. Invocation releases its
            // entry lock before user code; completion never takes the registry.
            // Serialize all closers with invalidation, not only with detachment.
            for (const auto& [event, listeners] : state_->channels)
                for (const auto& listener : listeners)
                    invalidate(listener);
            detached.swap(state_->channels);
        }
        // Callback captures are released outside registry and listener locks.
        // In-flight callbacks retain their entry until their guard leaves.
    }

    EventStats EventBus::diagnostics() const {
        std::lock_guard lock(state_->mutex);
        const auto& counters = *state_->counters;
        return {counters.domain, state_->registrations, state_->active, state_->dispatches,
                counters.invocations.load(std::memory_order_relaxed), counters.queued_failures.load(std::memory_order_relaxed),
                counters.discarded.load(std::memory_order_relaxed), state_->closed};
    }

    void EventBus::report_diagnostics() const noexcept {
        try {
            const auto summary = diagnostics();
            CE_LOG_DEBUG(CE::enginelog, "subsystem=events domain={} operation=summary registrations={} active={} dispatches={} invocations={} queued_failures={} discarded={} closed={}",
                         summary.domain, summary.registrations, summary.active, summary.dispatches, summary.invocations,
                         summary.queued_failures, summary.discarded, summary.closed);
        } catch (...) {
            Diagnostics::report_failure("event diagnostic snapshot", std::current_exception());
        }
    }
}
