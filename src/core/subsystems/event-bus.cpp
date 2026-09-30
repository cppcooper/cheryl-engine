#include <core/subsystems/event-bus.h>
#include <internals/exceptions.h>

#include <limits>
#include <algorithm>
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

    EventBus::Registration EventBus::register_listener(const std::string& event, Callback callback) {
        if (!callback)
            throw Exceptions::invalid_args(CE_HERE, "An event listener requires a callback");
        auto listener = std::make_shared<Listener>();
        listener->event = event;
        listener->callback = std::move(callback);
        std::lock_guard lock(state_->mutex);
        if (state_->closed)
            throw Exceptions::failed_operation(CE_HERE, "EventBus is closed");
        if (state_->next_id == std::numeric_limits<std::uint64_t>::max())
            throw Exceptions::failed_operation(CE_HERE, "Event registration IDs exhausted");
        const auto id = state_->next_id;
        state_->channels[event].push_back(listener);
        ++state_->next_id;
        return Registration(state_, listener, id);
    }

    void EventBus::dispatch(const std::string& event, const std::any& payload) {
        std::vector<std::shared_ptr<Listener>> snapshot;
        {
            std::lock_guard lock(state_->mutex);
            if (state_->closed)
                throw Exceptions::failed_operation(CE_HERE, "EventBus is closed");
            const auto found = state_->channels.find(event);
            if (found == state_->channels.end())
                return;
            snapshot = found->second;
        }
        // Registry edits cannot invalidate this dispatch; no registry lock is
        // held while arbitrary callbacks register more listeners or dispatch.
        for (const auto& listener : snapshot)
            invoke(listener, payload);
    }

    void EventBus::invoke(const std::shared_ptr<Listener>& listener, const std::any& payload) {
        {
            std::lock_guard lock(listener->mutex);
            // Check and enter are one transaction with unregister. A snapshot
            // alone must not authorize a callback after invalidation returns.
            if (!listener->active)
                return;
            ++listener->running;
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
            const auto channel = state_->channels.find(listener->event);
            if (channel != state_->channels.end()) {
                removed = std::erase(channel->second, listener) != 0;
                if (channel->second.empty())
                    state_->channels.erase(channel);
            }
        }
        // Even a duplicate remover invalidates before returning: another
        // remover may have detached this entry but not yet acquired its mutex.
        invalidate(listener);
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
            detached.swap(state_->channels);
        }
        // Callback captures are released outside registry and listener locks.
        // In-flight callbacks retain their entry until their guard leaves.
        for (const auto& [event, listeners] : detached)
            for (const auto& listener : listeners)
                invalidate(listener);
    }
}
