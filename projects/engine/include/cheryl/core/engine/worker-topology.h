#pragma once

#include <cheryl/export.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace CE::Workers {
    /** Kind of cache reported by a topology provider. */
    enum class CacheType { Data, ///< Data-only cache.
        Instruction, ///< Instruction-only cache.
        Unified ///< Combined instruction/data cache.
    };

    /** Eligible logical CPUs sharing one physical-core identity within a snapshot. */
    struct Core {
        // IDs belong to one topology snapshot; cpus includes its eligible SMT siblings.
        unsigned int id = 0; ///< Provider-defined ID local to this snapshot, not an index or persistent hardware key.
        std::vector<unsigned int> cpus; ///< Eligible OS logical CPU IDs belonging to this domain; pool preparation sorts/deduplicates them.
    };

    /** One reported cache and the eligible CPUs sharing it; no allocation/residency guarantee. */
    struct CacheDomain {
        unsigned int id = 0; ///< Provider-defined ID local to this snapshot, not an index or persistent hardware key.
        unsigned int level = 0; ///< Positive reported cache level, such as 1 for L1; zero rejects during pool preparation.
        CacheType type = CacheType::Unified; ///< Instruction/data classification of this cache.
        std::optional<std::uint64_t> capacity_bytes; ///< Positive reported total cache capacity in bytes, or absent when unavailable.
        std::optional<unsigned int> line_bytes; ///< Positive reported cache-line size in bytes, or absent when unavailable.
        std::vector<unsigned int> cpus; ///< Eligible OS logical CPU IDs belonging to this domain; pool preparation sorts/deduplicates them.
    };

    /** Owned discovery snapshot intersected with inherited CPU eligibility; not live OS state. */
    struct Topology {
        // OS CPU IDs intersected with the provider creator's inherited affinity.
        std::vector<unsigned int> eligible_cpus; ///< Sorted OS CPU IDs intersected with the creator's inherited affinity.
        std::vector<Core> cores; ///< Reported physical cores restricted to eligible CPUs.
        std::vector<CacheDomain> caches; ///< Reported cache domains restricted to eligible CPUs.
        bool cpu_affinity = false; ///< Whether this provider can bind the calling thread under the required contract.
        std::string limitations; ///< Human-readable discovery/binding limitations; not a machine policy token.
    };

/** Application-supplied discovery and calling-thread CPU-binding boundary.
     * Methods must support concurrent pool workers. Retain the provider through pool shutdown; binding must preserve memory placement.
     * @see [Worker topology](cheryl-guide:runtime/worker-execution)
     */
    class CE_API TopologyProvider {
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

/**
 * @fn CE::Workers::TopologyProvider::~TopologyProvider()
 * @brief Release provider state after all pools using it have finished.
 * @see [Detailed guide](cheryl-guide:runtime/worker-execution)
 */

/**
 * @fn CE::Workers::Topology CE::Workers::TopologyProvider::topology() const
 * @brief Return an owned discovery snapshot.
 * @return Copyable topology data independent of subsequent native queries.
 * Implementations support concurrent worker calls; topology visibility alone does not establish binding or a speedup.
 * @see [Detailed guide](cheryl-guide:runtime/worker-execution)
 */

/**
 * @fn std::vector<unsigned int> CE::Workers::TopologyProvider::current_thread_cpus() const
 * @brief Query the calling thread's actual native CPU mask.
 * @return Sorted distinct OS CPU IDs.
 * Failure to establish a usable native mask must reject rather than invent eligibility.
 * @see [Detailed guide](cheryl-guide:runtime/worker-execution)
 */

/**
 * @fn void CE::Workers::TopologyProvider::bind_current_thread(const std::vector<unsigned int>& cpus) const
 * @brief Bind only the calling thread while preserving memory placement.
 * @param cpus Requested eligible OS CPU IDs.
 * The pool reads the native mask back before admitting the job. Unsupported/failed binding rejects; other threads and NUMA placement must not be silently changed.
 * @see [Detailed guide](cheryl-guide:runtime/worker-execution)
 */
