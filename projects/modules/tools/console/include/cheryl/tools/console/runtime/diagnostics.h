#pragma once

#include <cheryl/tools/console/registry.h>
#include <cheryl/core/engine/worker-pool.h>
#include <cheryl/core/logging/log.h>

namespace CE::Tools::Console::Runtime {
    /** Query-only owner observations, sampled during retained Session work.
     * Group status is a synchronized logical-group snapshot, not a barrier.
     * Logging backend fields and shared-pool counts are independently sampled,
     * not an atomic health/durability verdict. A null logger omits log commands.
     * Captures retain group/logger handles; the host retains the physical Pool.
     * All registration and progression remain on the Registry/Session owner.
     */
    class EngineObservations {
        Registration group_, backend_, queue_;
    public:
        EngineObservations(Registry&, std::string prefix, CE::Workers::Group,
            std::shared_ptr<CE::Logging::LogHandle> logger, Rights read_rights);
        EngineObservations(const EngineObservations&) = delete;
        EngineObservations& operator=(const EngineObservations&) = delete;
    };
}
