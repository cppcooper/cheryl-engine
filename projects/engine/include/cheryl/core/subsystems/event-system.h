#pragma once

#include <cheryl/export.h>

#include "event-bus.h"
#include <templates/singleton.h>

namespace CE::Events {
    /** Global access to the default persistent event bus.
     * Independent owned buses may isolate a subsystem or application lifecycle;
     * all buses use immediate producer-thread delivery unless explicitly adapted.
     */
    struct EventSystem : ::CE::State::Singleton<EventSystem> {
    private:
        ::CE::Events::EventBus bus_;

    public:
        using Callback = ::CE::Events::EventBus::Callback;
        using Registration = ::CE::Events::EventBus::Registration;
        using Delivery = ::CE::Events::EventBus::Delivery;
        using ErrorHandler = ::CE::Events::EventBus::ErrorHandler;

        EventSystem() = default;
        [[nodiscard]] ::CE::Events::EventBus& bus() { return bus_; }
        CE_API Registration register_listener(
            const std::string& event,
            Callback callback,
            Delivery delivery = Delivery{},
            ErrorHandler errors = ErrorHandler{}
        );
        CE_API void dispatch(const std::string& event, const std::any& payload);
        template <::CE::Events::EventPayload Payload>
        Registration register_listener(
            const ::CE::Events::EventChannel<Payload>& event,
            typename ::CE::Events::EventChannel<Payload>::Callback callback,
            Delivery delivery = Delivery{},
            ErrorHandler errors = ErrorHandler{}
        ) {
            return bus_.register_listener(event, std::move(callback), std::move(delivery), std::move(errors));
        }
        template <::CE::Events::EventPayload Payload, typename Value>
            requires std::same_as<std::remove_cvref_t<Value>, Payload> && std::is_constructible_v<Payload, Value&&>
        void dispatch(const ::CE::Events::EventChannel<Payload>& event, Value&& payload) {
            bus_.dispatch(event, std::forward<Value>(payload));
        }
        bool unregister_listener(const Registration& registration) { return bus_.unregister_listener(registration); }
        void wait_for_listener(const Registration& registration) const { bus_.wait_for_listener(registration); }
        bool unregister_and_wait(const Registration& registration) { return bus_.unregister_and_wait(registration); }
    };
}

namespace CE::State {
    template <> CE_API auto Singleton<CE::Events::EventSystem>::storage() -> Storage&;
}
