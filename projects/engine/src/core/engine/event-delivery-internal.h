#pragma once

#include <core/engine/event-delivery.h>

#include <functional>

namespace CE::Events::Detail {
    // Internal submission seam for controlled cancellation fixtures. Production
    // uses CE::Workers::Group::submit. A submitter may enqueue or discard the callable,
    // or throw without retaining it; it must never invoke it inline or wait for execution.
    using WorkerSubmission = std::function<void(CE::Events::EventBus::Work)>;

    [[nodiscard]] CE::Events::EventBus::Delivery worker_stream_delivery(CE::Workers::Group group, WorkerSubmission submit);
}
