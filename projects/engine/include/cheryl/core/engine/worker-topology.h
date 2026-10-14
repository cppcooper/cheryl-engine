#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace CE::Workers {
    enum class CacheType { Data, Instruction, Unified };

    struct Core {
        // IDs belong to one topology snapshot; cpus includes its eligible SMT siblings.
        unsigned int id = 0;
        std::vector<unsigned int> cpus;
    };

    struct CacheDomain {
        unsigned int id = 0;
        unsigned int level = 0;
        CacheType type = CacheType::Unified;
        std::optional<std::uint64_t> capacity_bytes;
        std::optional<unsigned int> line_bytes;
        std::vector<unsigned int> cpus;
    };

    struct Topology {
        // OS CPU IDs intersected with the provider creator's inherited affinity.
        std::vector<unsigned int> eligible_cpus;
        std::vector<Core> cores;
        std::vector<CacheDomain> caches;
        bool cpu_affinity = false;
        std::string limitations;
    };

    class TopologyProvider {
    public:
        virtual ~TopologyProvider() = default;
        // Owned discovery snapshot. Methods must support concurrent pool workers.
        [[nodiscard]] virtual Topology topology() const = 0;
        // Sorted, distinct OS CPU IDs for the calling thread's actual native mask.
        [[nodiscard]] virtual std::vector<unsigned int> current_thread_cpus() const = 0;
        // Bind only the calling thread, without changing memory placement. The pool
        // reads its actual mask back before allowing a job to execute.
        virtual void bind_current_thread(const std::vector<unsigned int>& cpus) const = 0;
    };
}
