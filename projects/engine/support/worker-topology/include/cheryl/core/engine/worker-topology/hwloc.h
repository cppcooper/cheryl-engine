#pragma once

#include <core/engine/worker-topology.h>

#include <memory>

namespace CE::Workers::Hwloc {
    // Discover the executing machine when called; does not bind the creator or
    // place memory. Retain this instance in CE::Workers::PoolOptions::topology_provider.
    [[nodiscard]] std::shared_ptr<CE::Workers::TopologyProvider> make_topology_provider();
}
