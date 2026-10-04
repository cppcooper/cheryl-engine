#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "worker-affinity.h"
#include <internals/exceptions.h>

#include <format>
#include <system_error>
#include <utility>

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

namespace CE::Engine::WorkerDetail {
    std::vector<unsigned int> current_affinity() {
#if defined(__linux__)
        cpu_set_t mask;
        CPU_ZERO(&mask);
        const auto error = pthread_getaffinity_np(pthread_self(), sizeof(mask), &mask);
        if (error != 0)
            throw Exceptions::failed_operation(
                CE_HERE, std::format("Reading worker CPU affinity failed: {}", std::error_code(error, std::generic_category()).message())
            );
        std::vector<unsigned int> cpus;
        for (unsigned int cpu = 0; cpu < CPU_SETSIZE; ++cpu)
            if (CPU_ISSET(cpu, &mask))
                cpus.push_back(cpu);
        if (cpus.empty())
            throw Exceptions::failed_operation(CE_HERE, "Worker has no eligible CPU");
        return cpus;
#else
        throw Exceptions::failed_operation(CE_HERE, "Native worker affinity is unsupported on this target");
#endif
    }

    WorkerCapabilities discover_capabilities(const WorkerNativeAdapter& adapter) {
        WorkerCapabilities capabilities;
        if (adapter.cpu_affinity) {
            try {
                capabilities.available_cpus = adapter.query_affinity();
                if (capabilities.available_cpus.empty())
                    throw Exceptions::failed_operation(CE_HERE, "Worker has no eligible CPU");
                capabilities.cpu_affinity = true;
                capabilities.limitations = "Fixed CPU_SETSIZE mask; no automatic cache-domain or NUMA memory-placement discovery";
            } catch (...) {
                capabilities.limitations = "Cannot query the inherited eligible CPU mask; hard affinity is unavailable";
            }
        } else {
            capabilities.limitations = "Native affinity adapter unavailable; hard CPU requirements reject";
        }
        return capabilities;
    }

    void set_native_affinity(const std::vector<unsigned int>& cpus) {
#if defined(__linux__)
        cpu_set_t mask;
        CPU_ZERO(&mask);
        if (cpus.empty())
            throw Exceptions::invalid_args(CE_HERE, "Worker affinity requires an eligible CPU");
        for (const auto cpu : cpus) {
            if (cpu >= CPU_SETSIZE)
                throw Exceptions::invalid_args(CE_HERE, "CPU ID exceeds native affinity mask capacity");
            CPU_SET(cpu, &mask);
        }
        const auto error = pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask);
        if (error != 0)
            throw Exceptions::failed_operation(
                CE_HERE, std::format("Setting worker CPU affinity failed: {}", std::error_code(error, std::generic_category()).message())
            );
#else
        (void)cpus;
        throw Exceptions::failed_operation(CE_HERE, "Native worker affinity is unsupported on this target");
#endif
    }

    void apply_affinity(const WorkerNativeAdapter& adapter, const std::vector<unsigned int>& cpus) {
        adapter.set_affinity(cpus);
        // Linux may silently intersect the request with cpuset restrictions.
        // Verify the effective mask before allowing the requested job to run.
        if (adapter.query_affinity() != cpus)
            throw Exceptions::failed_operation(CE_HERE, "Effective worker CPU affinity differs from requested eligibility");
    }

    WorkerNativeAdapter native_worker_adapter() {
        WorkerNativeAdapter adapter;
#if defined(__linux__)
        adapter.cpu_affinity = true;
#endif
        adapter.query_affinity = current_affinity;
        adapter.set_affinity = set_native_affinity;
        adapter.start_thread = [](std::function<void()> work) { return std::thread(std::move(work)); };
        return adapter;
    }
}
