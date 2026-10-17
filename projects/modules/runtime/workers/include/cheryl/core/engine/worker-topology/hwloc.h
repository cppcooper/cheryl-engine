#pragma once

#include <cheryl/export.h>

#include <core/engine/worker-topology.h>

#include <memory>

namespace CE::Workers::Hwloc {
    // Discover the executing machine when called; does not bind the creator or
    // place memory. Retain this instance in CE::Workers::PoolOptions::topology_provider.
    [[nodiscard]] CE_WORKER_TOPOLOGY_HWLOC_API std::shared_ptr<CE::Workers::TopologyProvider> make_topology_provider();
}
