#pragma once

#include <core/engine/worker-topology.h>

namespace CE::Workers::Detail {
    // Validate and intersect a provider snapshot with the pool creator's mask.
    [[nodiscard]] CE::Workers::Topology prepare_topology(const CE::Workers::TopologyProvider& provider);
    [[nodiscard]] bool complete_core_topology(const CE::Workers::Topology& topology);
}
