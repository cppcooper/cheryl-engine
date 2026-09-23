#pragma once
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
 *
 * TODO: Consider typed event channels/IDs whose payload type is encoded in the API. String names
 * plus std::any move typo and payload-mismatch detection entirely to runtime listeners.
 */

namespace CE::SubSystems {
    struct EventSystem : Singleton_CTS<EventSystem> {
        using Callback = std::function<void(std::any)>;
        EventSystem() = default;
        void dispatch(const std::string &event, const std::any &payload);
        // TODO: Return a subscription/token or provide unregister support before finite-lifetime
        // objects rely on this system; registered callbacks currently have no removal mechanism.
        void register_listener(const std::string &event, const Callback &callback);
    protected:
        std::shared_mutex mtx;
        std::unordered_map<std::string, std::vector<Callback>> event_listeners;
    };
}
