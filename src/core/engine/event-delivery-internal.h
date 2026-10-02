#pragma once

#include <core/engine/event-delivery.h>

#include <functional>

namespace CE::Engine::DeliveryDetail {
    // Internal submission seam for controlled cancellation fixtures. Production
    // uses WorkerGroup::submit. A submitter may enqueue or discard the callable,
    // or throw without retaining it; it must never invoke it inline or wait for execution.
    using WorkerSubmission = std::function<void(SubSystems::EventBus::Work)>;

    [[nodiscard]] SubSystems::EventBus::Delivery worker_stream_delivery(
        WorkerGroup group,
        WorkerSubmission submit
    );
}
