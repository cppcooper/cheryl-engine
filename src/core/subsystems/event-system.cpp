#include <core/subsystems/event-system.h>

namespace CE::SubSystems {
    void EventSystem::dispatch(const std::string &event, const std::any &payload) {
        std::shared_lock rl(mtx);
        if (event_listeners.contains(event)) {
            const auto& listeners = event_listeners[event];
            for(auto& cb : listeners) {
                cb(payload);
            }
        }
    }

    void EventSystem::register_listener(const std::string &event, const Callback &callback) {
        std::unique_lock wl(mtx);
        event_listeners[event].push_back(callback);
    }
}
