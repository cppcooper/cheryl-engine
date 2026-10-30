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
        /** Default-bus alias with the identical EventBus::Callback contract. */
        using Callback = ::CE::Events::EventBus::Callback;
        /** Default-bus alias with the identical EventBus::Registration contract. */
        using Registration = ::CE::Events::EventBus::Registration;
        /** Default-bus alias with the identical EventBus::Delivery contract. */
        using Delivery = ::CE::Events::EventBus::Delivery;
        /** Default-bus alias with the identical EventBus::ErrorHandler contract. */
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

/**
 * @fn CE::Events::EventSystem::EventSystem()
 * @brief Construct a facade owning its own bus; Singleton access selects the process default facade.
 * @see [Detailed guide](cheryl-guide:runtime/event-delivery)
 */

/**
 * @fn CE::Events::EventBus& CE::Events::EventSystem::bus()
 * @brief Borrow the facade's bus without changing its delivery thread or listener lifetime.
 * @see [Detailed guide](cheryl-guide:runtime/event-delivery)
 */

/**
 * @fn Registration CE::Events::EventSystem::register_listener(const std::string& event, Callback callback, Delivery delivery, ErrorHandler errors)
 * @brief Forward to the owned bus's register_listener operation.
 * @see CE::Events::EventBus::register_listener
 * The payload, error-delivery, invalidation and wait contracts are identical; global access does not choose a callback thread.
 * @see [Detailed guide](cheryl-guide:runtime/event-delivery)
 */

/**
 * @fn void CE::Events::EventSystem::dispatch(const std::string& event, const std::any& payload)
 * @brief Forward to the owned bus's dispatch operation.
 * @see CE::Events::EventBus::dispatch
 * The payload, error-delivery, invalidation and wait contracts are identical; global access does not choose a callback thread.
 * @see [Detailed guide](cheryl-guide:runtime/event-delivery)
 */

/**
 * @fn bool CE::Events::EventSystem::unregister_listener(const Registration& registration)
 * @brief Forward to the owned bus's unregister_listener operation.
 * @see CE::Events::EventBus::unregister_listener
 * The payload, error-delivery, invalidation and wait contracts are identical; global access does not choose a callback thread.
 * @see [Detailed guide](cheryl-guide:runtime/event-delivery)
 */

/**
 * @fn void CE::Events::EventSystem::wait_for_listener(const Registration& registration) const
 * @brief Forward to the owned bus's wait_for_listener operation.
 * @see CE::Events::EventBus::wait_for_listener
 * The payload, error-delivery, invalidation and wait contracts are identical; global access does not choose a callback thread.
 * @see [Detailed guide](cheryl-guide:runtime/event-delivery)
 */

/**
 * @fn bool CE::Events::EventSystem::unregister_and_wait(const Registration& registration)
 * @brief Forward to the owned bus's unregister_and_wait operation.
 * @see CE::Events::EventBus::unregister_and_wait
 * The payload, error-delivery, invalidation and wait contracts are identical; global access does not choose a callback thread.
 * @see [Detailed guide](cheryl-guide:runtime/event-delivery)
 */

/**
 * @fn template<CE::Events::EventPayload Payload> CE::Events::EventSystem::Registration CE::Events::EventSystem::register_listener(const CE::Events::EventChannel<Payload>& event, typename CE::Events::EventChannel<Payload>::Callback callback, Delivery delivery, ErrorHandler errors)
 * @brief Register an owned typed listener on the default facade's bus.
 * @see CE::Events::EventBus::register_listener
 * Exact name/type isolation, persistent identifiers and deferred error/capture lifetime are unchanged.
 * @see [Detailed guide](cheryl-guide:runtime/event-delivery)
 */

/**
 * @fn template<CE::Events::EventPayload Payload, typename Value> void CE::Events::EventSystem::dispatch(const CE::Events::EventChannel<Payload>& event, Value&& payload)
 * @brief Forward exact-type owned payload dispatch to this facade's bus.
 * @see CE::Events::EventBus::dispatch
 * Payload construction, immediate exceptions and deferred error reporting obey the owned bus contract.
 * @see [Detailed guide](cheryl-guide:runtime/event-delivery)
 */
