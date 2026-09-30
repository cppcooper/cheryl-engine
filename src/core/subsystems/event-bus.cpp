#include <core/subsystems/event-bus.h>
#include <internals/exceptions.h>

#include <limits>
#include <utility>

namespace CE::SubSystems {
    EventBus::Registration EventBus::register_listener(const std::string& event, Callback callback) {
        if (!callback)
            throw Exceptions::invalid_args(CE_HERE, "An event listener requires a callback");
        auto listener = std::make_shared<Listener>();
        listener->event = event;
        listener->callback = std::move(callback);
        std::lock_guard lock(state_->mutex);
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
            const auto found = state_->channels.find(event);
            if (found == state_->channels.end())
                return;
            snapshot = found->second;
        }
        // Registry edits cannot invalidate this dispatch; no registry lock is
        // held while arbitrary callbacks register more listeners or dispatch.
        for (const auto& listener : snapshot)
            listener->callback(payload);
    }
}
