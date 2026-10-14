#pragma once

#include "platform-dispatcher.h"
#include "simulation-dispatcher.h"
#include "worker-pool.h"
#include <core/subsystems/event-bus.h>

namespace CE::Events {
    /** Optional composition adapters. EventBus itself includes none of the
     * dispatch services and remains immediate by default. Accepted task cancellation
     * is reported by EventBus's owned delivery ticket, not a discarded future.
     */
    [[nodiscard]] EventBus::Delivery platform_delivery(CE::Runtime::PlatformDispatcher::Submission endpoint);
    [[nodiscard]] EventBus::Delivery simulation_delivery(CE::Runtime::SimulationDispatcher::Submission endpoint);
    // One serial stream on shared CE::Workers::Group capacity. Copies of the returned
    // callable share ordering; separate calls create independently concurrent streams.
    [[nodiscard]] EventBus::Delivery worker_delivery(CE::Workers::Group group);
}
