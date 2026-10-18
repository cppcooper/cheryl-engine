#pragma once

#include <cheryl/export.h>

#include <core/engine/worker-topology.h>

#include <memory>

namespace CE::Workers::Hwloc {
    // Discover the executing machine when called; does not bind the creator or
    // place memory. Retain this instance in CE::Workers::PoolOptions::topology_provider.
    [[nodiscard]] CE_WORKER_TOPOLOGY_HWLOC_API std::shared_ptr<CE::Workers::TopologyProvider> make_topology_provider();
}

/**
 * @fn std::shared_ptr<CE::Workers::TopologyProvider> CE::Workers::Hwloc::make_topology_provider()
 * @brief Discover the executing machine through the optional hwloc module.
 * @return Owned provider to retain in PoolOptions::topology_provider through pool shutdown.
 * Loads CPU/core/cache discovery at this call, with no creator binding or memory placement. Allocation/discovery/API incompatibility failures propagate. Unsupported binding is reported through capabilities/limitations; Windows currently reports binding unavailable. Link Cheryl::Workers with hwloc enabled; no hwloc types cross this interface.
 * @see [Detailed guide](cheryl-guide:modules/workers)
 */
