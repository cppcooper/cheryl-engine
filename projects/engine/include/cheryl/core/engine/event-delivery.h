#pragma once

#include <cheryl/export.h>

#include "platform-dispatcher.h"
#include "simulation-dispatcher.h"
#include "worker-pool.h"
#include <core/subsystems/event-bus.h>

namespace CE::Events {
    /** Optional composition adapters. EventBus itself includes none of the
     * dispatch services and remains immediate by default. Accepted task cancellation
     * is reported by EventBus's owned delivery ticket, not a discarded future.
     */
    [[nodiscard]] CE_API EventBus::Delivery platform_delivery(CE::Runtime::PlatformDispatcher::Submission endpoint);
    [[nodiscard]] CE_API EventBus::Delivery simulation_delivery(CE::Runtime::SimulationDispatcher::Submission endpoint);
    // One serial stream on shared CE::Workers::Group capacity. Copies of the returned
    // callable share ordering; separate calls create independently concurrent streams.
    [[nodiscard]] CE_API EventBus::Delivery worker_delivery(CE::Workers::Group group);
}

/**
 * @fn CE::Events::EventBus::Delivery CE::Events::platform_delivery(CE::Runtime::PlatformDispatcher::Submission endpoint)
 * @brief Adapt an owned platform submission endpoint to deferred event delivery.
 * @param endpoint Copyable endpoint retained by the returned callable; requires active queue acceptance.
 * @return Nonblocking transport preserving queue order without retaining a borrowed dispatcher/context.
 * Loss of accepted work reaches an active listener's EventBus error sink; explicit unregister/close instead discards it. a discarded submission future is not the event error authority.
 * @see [Detailed guide](cheryl-guide:runtime/event-delivery)
 */

/**
 * @fn CE::Events::EventBus::Delivery CE::Events::simulation_delivery(CE::Runtime::SimulationDispatcher::Submission endpoint)
 * @brief Adapt a simulation submission endpoint to deferred event delivery.
 * @param endpoint Retained producer endpoint requiring active simulation queue acceptance.
 * @return Nonblocking transport; callbacks execute at simulation update boundaries.
 * EventBus owns deferred invocation and active-listener cancellation errors and captured-target lifetime independently of a discarded dispatcher future.
 * @see [Detailed guide](cheryl-guide:runtime/event-delivery)
 */

/**
 * @fn CE::Events::EventBus::Delivery CE::Events::worker_delivery(CE::Workers::Group group)
 * @brief Create one serial deferred stream on shared worker capacity.
 * @param group Copyable workload handle; keep its physical pool alive until delivery settles.
 * @return Owned transport; copies share FIFO ordering while separate factory calls create independently concurrent streams.
 * Submission rejection/active-listener cancellation is reported through EventBus delivery tickets; explicit invalidation intentionally discards pending delivery. An error sink must not depend on listener invalidation waiting for queued capture destruction.
 * @see [Detailed guide](cheryl-guide:runtime/event-delivery)
 */
