#pragma once

#include "platform-dispatcher.h"
#include "simulation-dispatcher.h"
#include "worker-pool.h"
#include <core/subsystems/event-bus.h>

namespace CE::Engine {
    /** Optional composition adapters. EventBus itself includes none of the
     * dispatch services and remains immediate by default. Accepted task cancellation
     * is reported by EventBus's owned delivery ticket, not a discarded future.
     */
    [[nodiscard]] SubSystems::EventBus::Delivery platform_event_delivery(
        PlatformDispatcher::Submission endpoint
    );
    [[nodiscard]] SubSystems::EventBus::Delivery simulation_event_delivery(
        SimulationDispatcher::Submission endpoint
    );
    // One serial stream on shared WorkerGroup capacity. Copies of the returned
    // callable share ordering; separate calls create independently concurrent streams.
    [[nodiscard]] SubSystems::EventBus::Delivery worker_event_delivery(
        WorkerGroup group
    );
}
