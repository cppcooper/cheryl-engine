#pragma once

#include <core/engine/worker-pool.h>

namespace CE::Engine::WorkerDetail {
    WorkerCapabilities discover_capabilities();
    std::vector<unsigned int> current_affinity();
    void apply_affinity(const std::vector<unsigned int>& cpus);
}
