#include <core/engine/worker-pool.h>
#include <core/engine/worker-topology/hwloc.h>

#if defined(HWLOC_API_VERSION)
#error The provider header must keep hwloc private.
#endif

#include "../hwloc-access.h"
#include <gtest/gtest.h>
#include <testing/worker-topology.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <future>
#include <iostream>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <thread>
#include <vector>

#if defined(__linux__)
#include <cerrno>
#include <climits>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace {
    std::vector<unsigned int> cpu_mask() {
#if defined(__linux__)
        for (std::size_t words = 16; words <= 16384; words *= 2) {
            std::vector<unsigned long> mask(words);
            if (syscall(SYS_sched_getaffinity, 0, words * sizeof(unsigned long), mask.data()) >= 0) {
                std::vector<unsigned int> result;
                for (std::size_t index = 0; index < words * sizeof(unsigned long) * CHAR_BIT; ++index)
                    if (mask[index / (sizeof(unsigned long) * CHAR_BIT)] & (1ul << (index % (sizeof(unsigned long) * CHAR_BIT))))
                        result.push_back(static_cast<unsigned int>(index));
                return result;
            }
            if (errno != EINVAL)
                break;
        }
        throw std::runtime_error("Cannot read the native Linux thread mask");
#else
        return CE::Workers::Hwloc::make_topology_provider()->current_thread_cpus();
#endif
    }

    std::uint64_t workload(std::vector<std::uint32_t>& data) {
        std::uint64_t result = 0;
        for (int pass = 0; pass < 8; ++pass)
            for (auto& value : data) {
                value = value * 1664525u + 1013904223u;
                result += value;
            }
        return result;
    }
}

TEST(worker_hwloc, discovery) {
    const auto before = cpu_mask();
    const auto provider = CE::Workers::Hwloc::make_topology_provider();
    const auto topology = provider->topology();
    EXPECT_EQ(cpu_mask(), before);
    EXPECT_TRUE(std::ranges::is_sorted(topology.eligible_cpus));
    EXPECT_TRUE(std::ranges::includes(before, topology.eligible_cpus));
    if (!topology.cpu_affinity)
        GTEST_SKIP() << topology.limitations;
    ASSERT_FALSE(topology.eligible_cpus.empty());
    ASSERT_FALSE(topology.cores.empty());
    ASSERT_FALSE(topology.caches.empty());
    for (const auto& cache : topology.caches) {
        EXPECT_GT(cache.level, 0u);
        EXPECT_FALSE(cache.cpus.empty());
        EXPECT_TRUE(std::ranges::includes(topology.eligible_cpus, cache.cpus));
    }
    std::cout << "Native topology: " << topology.cores.size() << " cores, " << topology.eligible_cpus.size() << " logical CPUs, "
              << topology.caches.size() << " cache domains\n";
}

TEST(worker_hwloc, inherited) {
    const auto creator_mask = cpu_mask();
    const auto provider = CE::Workers::Hwloc::make_topology_provider();
    if (!provider->topology().cpu_affinity)
        GTEST_SKIP() << provider->topology().limitations;
    auto creator = std::async(std::launch::async, [&] {
        provider->bind_current_thread({creator_mask.front()});
        const auto narrowed = CE::Workers::Hwloc::make_topology_provider();
        EXPECT_EQ(narrowed->topology().eligible_cpus, (std::vector<unsigned int>{creator_mask.front()}));
        CE::Workers::Pool pool(CE::Workers::PoolOptions{0, narrowed});
        EXPECT_EQ(pool.worker_count(), 1u);
        auto group = pool.make_group();
        EXPECT_EQ(group.submit(cpu_mask).get(), (std::vector<unsigned int>{creator_mask.front()}));
    });
    creator.get();
    EXPECT_EQ(cpu_mask(), creator_mask);
}

TEST(worker_hwloc, worker_masks) {
    const auto provider = CE::Workers::Hwloc::make_topology_provider();
    const auto topology = provider->topology();
    if (!topology.cpu_affinity || topology.cores.size() < 2)
        GTEST_SKIP() << "Two eligible physical cores with thread binding are required: " << topology.limitations;
    CE::Workers::Pool pool(CE::Workers::PoolOptions{2, provider});
    CE::Workers::GroupOptions options;
    options.cpu.locality = CE::Workers::LocalityPolicy::Spread;
    options.cpu.strength = CE::Workers::PolicyStrength::Required;
    auto group = pool.make_group(options);
    std::promise<std::vector<unsigned int>> first_mask;
    CE::Testing::WorkerRelease release(pool);
    auto first = group.submit([&] {
        first_mask.set_value(cpu_mask());
        release.ready.wait();
    });
    auto entered = first_mask.get_future();
    ASSERT_EQ(entered.wait_for(std::chrono::seconds{3}), std::future_status::ready);
    const auto mask_a = entered.get();
    auto second = group.submit(cpu_mask);
    const auto second_ready = second.wait_for(std::chrono::seconds{3});
    release.release();
    group.close();
    group.drain();
    first.get();
    ASSERT_EQ(second_ready, std::future_status::ready);
    const auto mask_b = second.get();
    ASSERT_EQ(mask_a.size(), 1u);
    ASSERT_EQ(mask_b.size(), 1u);
    EXPECT_TRUE(std::ranges::includes(topology.eligible_cpus, mask_a));
    EXPECT_TRUE(std::ranges::includes(topology.eligible_cpus, mask_b));
    EXPECT_NE(mask_a, mask_b);
    for (const auto& core : topology.cores)
        EXPECT_FALSE(
            std::ranges::find(core.cpus, mask_a.front()) != core.cpus.end() &&
            std::ranges::find(core.cpus, mask_b.front()) != core.cpus.end()
        );
}

TEST(worker_hwloc, memory_policy) {
    const auto provider = CE::Workers::Hwloc::make_topology_provider();
    const auto topology = provider->topology();
    if (!topology.cpu_affinity)
        GTEST_SKIP() << topology.limitations;
    const CE::Workers::Hwloc::Testing::MemoryProbe native;
    if (!native.capture())
        GTEST_SKIP() << "The OS cannot observe this thread's memory binding; CPU binding acceptance remains separate";
    auto worker = std::async(std::launch::async, [&] {
        const auto before = native.capture();
        ASSERT_TRUE(before);
        provider->bind_current_thread({topology.eligible_cpus.front()});
        EXPECT_EQ(cpu_mask(), (std::vector<unsigned int>{topology.eligible_cpus.front()}));
        const auto after = native.capture();
        ASSERT_TRUE(after);
        EXPECT_EQ(before->policy(), after->policy());
        EXPECT_TRUE(before->same_nodes(*after));
    });
    worker.get();
}

TEST(worker_hwloc, workloads) {
    const auto provider = CE::Workers::Hwloc::make_topology_provider();
    if (!provider->topology().cpu_affinity || provider->topology().cores.empty())
        GTEST_SKIP() << provider->topology().limitations;
    constexpr std::size_t jobs = 96;
    for (int selection = -1; selection < 3; ++selection) {
        CE::Workers::PoolOptions pool_options{2, selection < 0 ? nullptr : provider};
        CE::Workers::Pool pool(pool_options);
        for (const bool parallel : {false, true}) {
            CE::Workers::GroupOptions options;
            options.max_concurrency = parallel ? 2 : 1;
            if (selection >= 0)
                options.cpu.locality = static_cast<CE::Workers::LocalityPolicy>(selection);
            auto group = pool.make_group(options);
            std::vector<std::vector<std::uint32_t>> buffers(parallel ? jobs : 1, std::vector<std::uint32_t>(32768, 1));
            std::vector<std::future<std::uint64_t>> results;
            const auto start = std::chrono::steady_clock::now();
            for (std::size_t index = 0; index < jobs; ++index)
                results.push_back(group.submit([&, index] { return workload(buffers[parallel ? index : 0]); }));
            group.close();
            group.drain();
            const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
            std::uint64_t total = 0;
            for (auto& result : results)
                total += result.get();
            std::vector<std::uint32_t> reference(32768, 1);
            std::uint64_t expected = 0;
            if (parallel)
                expected = workload(reference) * jobs;
            else
                for (std::size_t index = 0; index < jobs; ++index)
                    expected += workload(reference);
            EXPECT_EQ(total, expected);
            EXPECT_EQ(group.status().policy_failures, 0u);
            const char* name = selection < 0 ? "ordinary" : selection == 0 ? "automatic" : selection == 1 ? "reuse" : "spread";
            std::cout << "Worker workload " << (parallel ? "parallel" : "serial") << ' ' << name << ": " << elapsed << " us\n";
        }
    }
}
