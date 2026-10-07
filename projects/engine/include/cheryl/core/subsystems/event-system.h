#pragma once

#include "event-bus.h"
#include <templates/singleton.h>

namespace CE::SubSystems {
    /** Global access to the default persistent event bus.
     * Independent owned buses may isolate a subsystem or application lifecycle;
     * all buses use immediate producer-thread delivery unless explicitly adapted.
     */
    struct EventSystem : Singleton_CTS<EventSystem> {
    private:
        EventBus bus_;

    public:
        using Callback = EventBus::Callback;
        using Registration = EventBus::Registration;
        using Delivery = EventBus::Delivery;
        using ErrorHandler = EventBus::ErrorHandler;

        EventSystem() = default;
        [[nodiscard]] EventBus& bus() { return bus_; }
        Registration register_listener(
            const std::string& event,
            Callback callback,
            Delivery delivery = Delivery{},
            ErrorHandler errors = ErrorHandler{}
        );
        void dispatch(const std::string& event, const std::any& payload);
        template <EventPayload Payload>
        Registration register_listener(
            const EventChannel<Payload>& event,
            typename EventChannel<Payload>::Callback callback,
            Delivery delivery = Delivery{},
            ErrorHandler errors = ErrorHandler{}
        ) {
            return bus_.register_listener(event, std::move(callback), std::move(delivery), std::move(errors));
        }
        template <EventPayload Payload, typename Value>
            requires std::same_as<std::remove_cvref_t<Value>, Payload> && std::is_constructible_v<Payload, Value&&>
        void dispatch(const EventChannel<Payload>& event, Value&& payload) {
            bus_.dispatch(event, std::forward<Value>(payload));
        }
        bool unregister_listener(const Registration& registration) { return bus_.unregister_listener(registration); }
        void wait_for_listener(const Registration& registration) const { bus_.wait_for_listener(registration); }
        bool unregister_and_wait(const Registration& registration) { return bus_.unregister_and_wait(registration); }
    };
}
