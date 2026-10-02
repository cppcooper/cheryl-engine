#pragma once

#include <core/engine/worker-pool.h>

#include <functional>
#include <cstddef>
#include <memory>
#include <thread>
#include <vector>

namespace CE::Engine::WorkerDetail {
    // Per-pool native operations, never a global test override. Production uses
    // pthread affinity and std::thread; recording fixtures can force failures.
    struct WorkerNativeAdapter {
        bool cpu_affinity = false;
        std::function<std::vector<unsigned int>()> query_affinity;
        std::function<void(const std::vector<unsigned int>&)> set_affinity;
        // Return an owned joinable thread, or throw without retaining the work.
        std::function<std::thread(std::function<void()>)> start_thread;
    };

    struct WorkerPoolAccess {
        [[nodiscard]] static std::unique_ptr<WorkerPool> create(
            std::size_t worker_count,
            WorkerNativeAdapter adapter
        );
    };
}
