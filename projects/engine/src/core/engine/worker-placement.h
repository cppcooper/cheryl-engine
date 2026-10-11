#pragma once

#include <core/engine/worker-pool.h>

#include <cstddef>
#include <optional>
#include <vector>

namespace CE::Engine::WorkerDetail {
    struct WorkerPlacementCpu {
        unsigned int cpu = 0;
        std::size_t core = 0;
        std::size_t running = 0;
        bool known_core = false;
    };

    struct WorkerPlacement {
        std::vector<WorkerPlacementCpu> cpus;
        std::vector<std::size_t> core_running;
    };

    struct WorkerGroupPlacement {
        std::vector<std::size_t> candidates;
        std::optional<std::size_t> last_target;
        std::size_t next_candidate = 0;
    };

    [[nodiscard]] WorkerPlacement make_placement(const WorkerTopology& topology);
    [[nodiscard]] WorkerGroupPolicy resolve_policy(
        const WorkerGroupOptions& options,
        const WorkerCapabilities& capabilities,
        const WorkerTopology& topology,
        const WorkerPlacement& placement,
        bool provider_selected,
        std::size_t capacity
    );
    [[nodiscard]] WorkerGroupPlacement make_group_placement(const WorkerGroupPolicy& policy, const WorkerPlacement& placement);
    // Scheduler lock held. Locality affects target choice after fair group selection.
    [[nodiscard]] std::size_t reserve_cpu(
        WorkerPlacement& placement,
        WorkerGroupPlacement& group,
        const WorkerGroupPolicy& policy,
        std::optional<unsigned int> current_cpu
    );
    void release_cpu(WorkerPlacement& placement, std::size_t target) noexcept;
    [[nodiscard]] std::optional<std::size_t> find_cpu(const WorkerPlacement& placement, unsigned int cpu);
}
