#include <core/engine/event-delivery.h>

#include <utility>

namespace CE::Engine {
    SubSystems::EventBus::Delivery platform_event_delivery(PlatformDispatcher::Submission endpoint) {
        return [endpoint = std::move(endpoint)](SubSystems::EventBus::Work work) {
            (void)endpoint.submit([work = std::move(work)](EngineContext&) mutable { work(); });
            return true;
        };
    }

    SubSystems::EventBus::Delivery simulation_event_delivery(SimulationDispatcher::Submission endpoint) {
        return [endpoint = std::move(endpoint)](SubSystems::EventBus::Work work) {
            (void)endpoint.submit(std::move(work));
            return true;
        };
    }
}
