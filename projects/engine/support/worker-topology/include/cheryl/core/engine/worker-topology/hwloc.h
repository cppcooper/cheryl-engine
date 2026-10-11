#pragma once

#include <core/engine/worker-topology.h>

#include <memory>

namespace CE::Engine {
    // Discover the executing machine when called; does not bind the creator or
    // place memory. Retain this instance in WorkerPoolOptions::topology_provider.
    [[nodiscard]] std::shared_ptr<WorkerTopologyProvider> make_hwloc_worker_topology();
}
