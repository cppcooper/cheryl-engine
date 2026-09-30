#pragma once

#include "event-bus.h"
#include <templates/singleton.h>

namespace CE::SubSystems {
    /** Global access to the default persistent event bus.
     * Independent owned buses may isolate a subsystem or application lifecycle;
     * all buses use immediate producer-thread delivery unless explicitly adapted.
     */
    struct EventSystem : Singleton_CTS<EventSystem> {
        EventBus bus_;

        using Callback = EventBus::Callback;
        using Registration = EventBus::Registration;

        EventSystem() = default;
        [[nodiscard]] EventBus& bus() { return bus_; }
        Registration register_listener(const std::string& event, Callback callback);
        void dispatch(const std::string& event, const std::any& payload);
    };
}
