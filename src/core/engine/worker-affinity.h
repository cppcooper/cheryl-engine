#pragma once

#include "worker-pool-internal.h"

#include <vector>

namespace CE::Engine::WorkerDetail {
    WorkerNativeAdapter native_worker_adapter();
    WorkerCapabilities discover_capabilities(const WorkerNativeAdapter& adapter);
    void apply_affinity(const WorkerNativeAdapter& adapter, const std::vector<unsigned int>& cpus);
}
