#include <core/subsystems/event-system.h>

#include <utility>

namespace CE::Events {
    void EventSystem::dispatch(const std::string& event, const std::any& payload) {
        bus_.dispatch(event, payload);
    }

    EventSystem::Registration
    EventSystem::register_listener(const std::string& event, Callback callback, Delivery delivery, ErrorHandler errors) {
        return bus_.register_listener(event, std::move(callback), std::move(delivery), std::move(errors));
    }
}

namespace CE::State {
    template <> auto Singleton<CE::Events::EventSystem>::storage() -> Storage& {
        static Storage value;
        return value;
    }
}
