#pragma once

#include <core/engine/worker-topology.h>

namespace CE::Engine::WorkerDetail {
    // Validate and intersect a provider snapshot with the pool creator's mask.
    [[nodiscard]] WorkerTopology prepare_topology(const WorkerTopologyProvider& provider);
    [[nodiscard]] bool complete_core_topology(const WorkerTopology& topology);
}
