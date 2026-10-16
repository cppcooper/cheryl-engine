#pragma once

#include "worker-pool-internal.h"

#include <vector>

namespace CE::Workers::Detail {
    NativeAdapter native_worker_adapter();
    CE::Workers::Capabilities discover_capabilities(const NativeAdapter& adapter);
    void apply_affinity(const NativeAdapter& adapter, const std::vector<unsigned int>& cpus);
}
