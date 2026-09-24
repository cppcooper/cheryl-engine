#include <core/subsystems/event-system.h>

namespace CE::SubSystems {
    void EventSystem::dispatch(const std::string &event, const std::any &payload) {
        // Snapshot the listener collection while protected so registration cannot
        // invalidate it, then release the lock before invoking arbitrary callback code.
        std::shared_lock rl(mtx);

        const auto iter = event_listeners.find(event);
        if (iter == event_listeners.end()) {
            return;
        }

        auto callbacks_snapshot{iter->second};
        rl.unlock();

        for(auto& cb : callbacks_snapshot) {
            cb(payload);
        }
    }

    void EventSystem::register_listener(const std::string &event, Callback callback) {
        std::unique_lock wl(mtx);
        event_listeners[event].push_back(std::move(callback));
    }
}
