#pragma once

#include "platform-dispatcher.h"
#include "simulation-dispatcher.h"
#include <core/subsystems/event-bus.h>

namespace CE::Engine {
    /** Optional composition adapters. EventBus itself includes none of the
     * dispatch services and remains immediate by default. Accepted task cancellation
     * is reported by EventBus's owned delivery ticket, not a discarded future.
     */
    [[nodiscard]] SubSystems::EventBus::Delivery platform_event_delivery(PlatformDispatcher::Submission endpoint);
    [[nodiscard]] SubSystems::EventBus::Delivery simulation_event_delivery(SimulationDispatcher::Submission endpoint);
}
