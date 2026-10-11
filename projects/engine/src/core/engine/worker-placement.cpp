#include "worker-placement.h"
#include <internals/exceptions.h>

#include <algorithm>
#include <iterator>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace CE::Engine {
    void validate_worker_group_options(const WorkerGroupOptions& options) {
        if (options.weight == 0 || options.weight > 1024 || options.priority > 7)
            throw Exceptions::invalid_args(CE_HERE, "Worker weight must be 1..1024 and priority 0..7");
        if (options.cpu.strength != WorkerPolicyStrength::Preferred && options.cpu.strength != WorkerPolicyStrength::Required)
            throw Exceptions::invalid_args(CE_HERE, "Unknown worker CPU policy strength");
        if (options.cpu.locality && *options.cpu.locality != WorkerLocalityPolicy::Automatic &&
            *options.cpu.locality != WorkerLocalityPolicy::Reuse && *options.cpu.locality != WorkerLocalityPolicy::Spread)
            throw Exceptions::invalid_args(CE_HERE, "Unknown worker locality policy");
        auto cpus = options.cpu.cpus;
        std::sort(cpus.begin(), cpus.end());
        if (std::adjacent_find(cpus.begin(), cpus.end()) != cpus.end())
            throw Exceptions::invalid_args(CE_HERE, "Worker CPU eligibility contains duplicates");
    }
}

namespace CE::Engine::WorkerDetail {
    namespace {
        std::vector<unsigned int> intersection(const std::vector<unsigned int>& left, const std::vector<unsigned int>& right) {
            std::vector<unsigned int> result;
            std::set_intersection(left.begin(), left.end(), right.begin(), right.end(), std::back_inserter(result));
            return result;
        }

        void fallback(WorkerGroupPolicy& policy, const std::string& reason) {
            if (!policy.limitations.empty())
                policy.limitations += "; ";
            policy.limitations += reason;
            policy.preferred_fallback = policy.requested.cpu.strength == WorkerPolicyStrength::Preferred;
        }
    }

    WorkerPlacement make_placement(const WorkerTopology& topology) {
        WorkerPlacement result;
        result.cpus.reserve(topology.eligible_cpus.size());
        result.core_running.resize(topology.cores.size());
        std::unordered_map<unsigned int, std::size_t> cores;
        for (std::size_t index = 0; index < topology.cores.size(); ++index)
            for (const auto cpu : topology.cores[index].cpus)
                cores.emplace(cpu, index);
        for (const auto cpu : topology.eligible_cpus) {
            const auto found = cores.find(cpu);
            const bool known = found != cores.end();
            const auto slot = known ? found->second : result.core_running.size();
            if (!known)
                result.core_running.push_back(0);
            result.cpus.push_back(WorkerPlacementCpu{cpu, slot, 0, known});
        }
        return result;
    }

    WorkerGroupPolicy resolve_policy(
        const WorkerGroupOptions& options,
        const WorkerCapabilities& capabilities,
        const WorkerTopology& topology,
        const WorkerPlacement& placement,
        const bool provider_selected,
        const std::size_t capacity
    ) {
        validate_worker_group_options(options);
        const bool hard = options.cpu.strength == WorkerPolicyStrength::Required;
        if (hard && options.cpu.numa_node)
            throw Exceptions::failed_operation(CE_HERE, "NUMA placement is unavailable and remains deferred");
        if (hard && !options.cpu.cpus.empty() && !capabilities.cpu_affinity)
            throw Exceptions::failed_operation(CE_HERE, "Required CPU affinity is unsupported");
        WorkerGroupPolicy policy;
        policy.requested = options;
        policy.affinity_supported = capabilities.cpu_affinity;
        policy.limitations = capabilities.limitations;
        policy.effective_cpus = capabilities.available_cpus;
        policy.effective_max_concurrency = options.max_concurrency == 0 ? capacity : std::min(options.max_concurrency, capacity);
        if (options.cpu.numa_node)
            fallback(policy, "Preferred NUMA placement is unavailable and remains deferred");
        if (!options.cpu.cpus.empty()) {
            auto requested = options.cpu.cpus;
            std::sort(requested.begin(), requested.end());
            if (capabilities.cpu_affinity) {
                auto allowed = intersection(requested, capabilities.available_cpus);
                if (hard && allowed != requested)
                    throw Exceptions::invalid_args(CE_HERE, "Required worker CPUs are not in the pool's eligible CPU set");
                if (!allowed.empty())
                    policy.effective_cpus = std::move(allowed);
                if (policy.effective_cpus != requested)
                    fallback(policy, "Preferred CPU set fell back to available eligibility");
            } else {
                fallback(policy, "Preferred explicit CPU affinity is unsupported");
            }
        }
        if (options.cpu.shared_cache_domain) {
            if (!capabilities.cpu_affinity || !capabilities.cache_topology) {
                if (hard)
                    throw Exceptions::failed_operation(CE_HERE, "Required cache-domain placement needs reported caches and thread binding");
                fallback(policy, "Preferred cache-domain placement is unavailable");
            } else {
                const auto domain = std::ranges::find(topology.caches, *options.cpu.shared_cache_domain, &WorkerCacheDomain::id);
                if (domain == topology.caches.end()) {
                    if (hard)
                        throw Exceptions::invalid_args(CE_HERE, "Required worker cache-domain ID is not in this pool's topology");
                    fallback(policy, "Preferred cache-domain ID is not in this pool's topology");
                } else {
                    auto allowed = intersection(policy.effective_cpus, domain->cpus);
                    if (allowed.empty()) {
                        if (hard)
                            throw Exceptions::invalid_args(CE_HERE, "Required cache domain conflicts with worker CPU eligibility");
                        fallback(policy, "Preferred cache domain conflicts with worker CPU eligibility");
                    } else {
                        policy.effective_cpus = std::move(allowed);
                        policy.effective_cache_domain = domain->id;
                    }
                }
            }
        }

        const auto candidates = make_group_placement(policy, placement);
        policy.core_topology =
            provider_selected && !candidates.candidates.empty() &&
            std::ranges::all_of(candidates.candidates, [&](const std::size_t target) { return placement.cpus[target].known_core; });
        const auto locality = options.cpu.locality ? options.cpu.locality
                              : provider_selected  ? std::optional{WorkerLocalityPolicy::Automatic}
                                                   : std::nullopt;
        if (locality) {
            if (!provider_selected || !capabilities.cpu_affinity || candidates.candidates.empty()) {
                if (hard && options.cpu.locality)
                    throw Exceptions::failed_operation(CE_HERE, "Required locality needs an eligible topology provider and thread binding");
                fallback(policy, "Cache/core locality fell back to ordinary CPU eligibility");
            } else {
                if (!policy.core_topology) {
                    if (hard && options.cpu.locality)
                        throw Exceptions::failed_operation(
                            CE_HERE, "Required locality needs physical-core information for all selected CPUs"
                        );
                    fallback(policy, "Physical-core information is incomplete; locality uses distinct logical CPUs where necessary");
                }
                policy.effective_locality = locality;
            }
        }
        return policy;
    }

    WorkerGroupPlacement make_group_placement(const WorkerGroupPolicy& policy, const WorkerPlacement& placement) {
        WorkerGroupPlacement result;
        for (std::size_t index = 0; index < placement.cpus.size(); ++index)
            if (std::binary_search(policy.effective_cpus.begin(), policy.effective_cpus.end(), placement.cpus[index].cpu))
                result.candidates.push_back(index);
        return result;
    }

    std::size_t reserve_cpu(
        WorkerPlacement& placement,
        WorkerGroupPlacement& group,
        const WorkerGroupPolicy& policy,
        const std::optional<unsigned int> current_cpu
    ) {
        const bool reuse = policy.effective_locality == WorkerLocalityPolicy::Reuse ||
                           (policy.effective_locality == WorkerLocalityPolicy::Automatic && policy.effective_max_concurrency == 1);
        const auto score = [&](const std::size_t candidate) {
            const auto target = group.candidates[candidate];
            const auto& cpu = placement.cpus[target];
            unsigned int locality = 2;
            if (reuse && group.last_target) {
                if (target == *group.last_target)
                    locality = 0;
                else if (cpu.core == placement.cpus[*group.last_target].core)
                    locality = 1;
            }
            const auto rotation = candidate >= group.next_candidate ? candidate - group.next_candidate
                                                                    : group.candidates.size() - group.next_candidate + candidate;
            // Busy-core/CPU load always wins over a soft reuse preference. Spread
            // rotates equal candidates; reuse also avoids needless worker migration.
            return std::tuple{placement.core_running[cpu.core], cpu.running, locality, reuse && current_cpu != cpu.cpu,
                reuse ? std::size_t{0} : rotation, reuse ? rotation : std::size_t{0}};
        };
        std::size_t selected = 0;
        for (std::size_t candidate = 1; candidate < group.candidates.size(); ++candidate)
            if (score(candidate) < score(selected))
                selected = candidate;
        const auto target = group.candidates[selected];
        auto& cpu = placement.cpus[target];
        ++cpu.running;
        ++placement.core_running[cpu.core];
        group.next_candidate = selected + 1 == group.candidates.size() ? 0 : selected + 1;
        return target;
    }

    void release_cpu(WorkerPlacement& placement, const std::size_t target) noexcept {
        auto& cpu = placement.cpus[target];
        --cpu.running;
        --placement.core_running[cpu.core];
    }

    std::optional<std::size_t> find_cpu(const WorkerPlacement& placement, const unsigned int cpu) {
        const auto found = std::ranges::lower_bound(placement.cpus, cpu, {}, &WorkerPlacementCpu::cpu);
        if (found == placement.cpus.end() || found->cpu != cpu)
            return std::nullopt;
        return static_cast<std::size_t>(found - placement.cpus.begin());
    }
}
