#pragma once

#include <cheryl/export.h>

#include "worker-pool-internal.h"

#include <vector>

namespace CE::Workers::Detail {
    CE_API NativeAdapter native_worker_adapter();
    CE_API CE::Workers::Capabilities discover_capabilities(const NativeAdapter& adapter);
    CE_API void apply_affinity(const NativeAdapter& adapter, const std::vector<unsigned int>& cpus);
}
