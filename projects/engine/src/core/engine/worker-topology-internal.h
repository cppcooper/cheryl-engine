#pragma once

#include <cheryl/export.h>

#include <core/engine/worker-topology.h>

namespace CE::Workers::Detail {
    // Validate and intersect a provider snapshot with the pool creator's mask.
    [[nodiscard]] CE_API CE::Workers::Topology prepare_topology(const CE::Workers::TopologyProvider& provider);
    [[nodiscard]] CE_API bool complete_core_topology(const CE::Workers::Topology& topology);
}
