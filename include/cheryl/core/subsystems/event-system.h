#pragma once
#ifndef EVENT_SYSTEM_H
#define EVENT_SYSTEM_H
#include <any>
#include <vector>
#include <functional>
#include <shared_mutex>
#include <templates/singleton.h>

/* Event System
 * Modeled as a publisher/subscriber pattern, allowing immediate notification to listeners.
 * Allows runtime event creation, in order to provide a light-weight extendable event system with very little overhead.
 * Utilizes std::any to deliver payloads of any form and size.
 *
 * Warning: this all means that dispatching an event with or without a specific payload has the potential
 * to break a listener's expectations and consequently blow up.
 *
 * So it is up to the listener to compensate for this risk.
 * Or on the developer to fucking write consistent [safe] event code.
 */

namespace CE::SubSystems {
    struct EventSystem : Singleton_CTS<EventSystem> {
        using Callback = std::function<void(std::any)>;
        EventSystem() = default;
        void dispatch(const std::string &event, const std::any &payload);
        void register_listener(const std::string &event, const Callback &callback);
    protected:
        std::shared_mutex mtx;
        std::unordered_map<std::string, std::vector<Callback>> event_listeners;
    };
}

#endif //EVENT_SYSTEM_H
