#include "worker-topology-internal.h"
#include <internals/exceptions.h>

#include <algorithm>
#include <iterator>
#include <new>
#include <unordered_set>
#include <utility>

namespace CE::Workers::Detail {
    namespace {
        void normalize_cpus(std::vector<unsigned int>& cpus) {
            std::sort(cpus.begin(), cpus.end());
            if (std::adjacent_find(cpus.begin(), cpus.end()) != cpus.end())
                throw Exceptions::invalid_args(CE_HERE, "Worker topology contains duplicate CPU IDs");
        }

        void restrict_cpus(std::vector<unsigned int>& cpus, const std::vector<unsigned int>& allowed) {
            normalize_cpus(cpus);
            std::vector<unsigned int> intersection;
            std::set_intersection(cpus.begin(), cpus.end(), allowed.begin(), allowed.end(), std::back_inserter(intersection));
            cpus = std::move(intersection);
        }

        void limitation(CE::Workers::Topology& topology, const std::string& reason) {
            if (!topology.limitations.empty())
                topology.limitations += "; ";
            topology.limitations += reason;
        }
    }

    bool complete_core_topology(const CE::Workers::Topology& topology) {
        std::size_t covered = 0;
        for (const auto& core : topology.cores)
            covered += core.cpus.size();
        return !topology.eligible_cpus.empty() && covered == topology.eligible_cpus.size();
    }

    CE::Workers::Topology prepare_topology(const CE::Workers::TopologyProvider& provider) {
        auto topology = provider.topology();
        normalize_cpus(topology.eligible_cpus);
        std::vector<unsigned int> inherited;
        try {
            inherited = provider.current_thread_cpus();
        } catch (const std::bad_alloc&) {
            throw;
        } catch (const std::exception& error) {
            topology.cpu_affinity = false;
            limitation(topology, "Cannot query the pool creator's inherited CPU mask: " + std::string(error.what()));
        } catch (...) {
            topology.cpu_affinity = false;
            limitation(topology, "Cannot query the pool creator's inherited CPU mask");
        }
        normalize_cpus(inherited);
        restrict_cpus(topology.eligible_cpus, inherited);
        if (topology.eligible_cpus.empty()) {
            topology.cpu_affinity = false;
            limitation(topology, "No verified eligible CPUs; hard affinity is unavailable");
        }
        if (!topology.cpu_affinity)
            limitation(topology, "Thread-specific CPU binding is unavailable");

        std::unordered_set<unsigned int> core_ids;
        std::unordered_set<unsigned int> core_cpus;
        for (auto& core : topology.cores) {
            if (!core_ids.insert(core.id).second)
                throw Exceptions::invalid_args(CE_HERE, "Worker topology contains duplicate core IDs");
            restrict_cpus(core.cpus, topology.eligible_cpus);
            for (const auto cpu : core.cpus)
                if (!core_cpus.insert(cpu).second)
                    throw Exceptions::invalid_args(CE_HERE, "A worker CPU belongs to multiple physical cores");
        }
        std::erase_if(topology.cores, [](const CE::Workers::Core& core) { return core.cpus.empty(); });

        std::unordered_set<unsigned int> cache_ids;
        for (auto& cache : topology.caches) {
            if (!cache_ids.insert(cache.id).second || cache.level == 0)
                throw Exceptions::invalid_args(CE_HERE, "Worker topology requires unique cache IDs and positive cache levels");
            if (cache.type != CE::Workers::CacheType::Data && cache.type != CE::Workers::CacheType::Instruction &&
                cache.type != CE::Workers::CacheType::Unified)
                throw Exceptions::invalid_args(CE_HERE, "Worker topology contains an unknown cache type");
            if ((cache.capacity_bytes && *cache.capacity_bytes == 0) || (cache.line_bytes && *cache.line_bytes == 0))
                throw Exceptions::invalid_args(CE_HERE, "Unknown cache sizes must be omitted, rather than zero");
            restrict_cpus(cache.cpus, topology.eligible_cpus);
        }
        std::erase_if(topology.caches, [](const CE::Workers::CacheDomain& cache) { return cache.cpus.empty(); });
        if (!complete_core_topology(topology))
            limitation(topology, "Physical-core/SMT information is incomplete; automatic sizing uses eligible logical CPUs");
        if (topology.caches.empty())
            limitation(topology, "No eligible cache domains were reported");
        return topology;
    }
}
