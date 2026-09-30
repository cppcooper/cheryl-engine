#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <gtest/gtest.h>

#include <core/engine/worker-pool.h>
#include <internals/exceptions.h>

#include <atomic>
#include <future>
#include <memory>
#include <stdexcept>
#include <vector>

#if defined(__linux__)
#include <sched.h>
#endif

TEST(worker_pool, owned_jobs_return_values_and_failures_without_stopping_other_work) {
    CE::Engine::WorkerPool pool(2);
    auto group = pool.make_group();
    auto value = group.submit([number = std::make_unique<int>(42)] { return *number; });
    auto failure = group.submit([]() -> int { throw std::runtime_error("failed job"); });
    auto next = group.submit([] { return 17; });
    group.close();
    group.drain();
    EXPECT_EQ(value.get(), 42);
    EXPECT_THROW(failure.get(), std::runtime_error);
    EXPECT_EQ(next.get(), 17);
    EXPECT_EQ(group.status().completed, 3u);
}

TEST(worker_pool, group_concurrency_one_preserves_fifo_while_groups_share_the_pool) {
    CE::Engine::WorkerPool pool(3);
    auto serial = pool.make_group({1});
    auto other = pool.make_group();
    std::vector<int> order;
    std::vector<std::future<void>> jobs;
    for (int i = 0; i < 8; ++i)
        jobs.push_back(serial.submit([&, i] { order.push_back(i); }));
    auto separate = other.submit([] { return 9; });
    serial.close();
    serial.drain();
    for (auto& job : jobs)
        job.get();
    EXPECT_EQ(order, (std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7}));
    EXPECT_EQ(separate.get(), 9);
    EXPECT_TRUE(other.status().accepting);
}

TEST(worker_pool, close_drains_accepted_jobs_and_saved_groups_reject_after_destruction) {
    auto pool = std::make_unique<CE::Engine::WorkerPool>();
    auto group = pool->make_group();
    std::atomic<int> calls{0};
    std::vector<std::future<void>> results;
    for (int i = 0; i < 5; ++i)
        results.push_back(group.submit([&] { ++calls; }));
    pool.reset();
    for (auto& result : results)
        result.get();
    EXPECT_EQ(calls.load(), 5);
    EXPECT_FALSE(group.status().accepting);
    EXPECT_THROW(static_cast<void>(group.submit([] {})), CE::Exceptions::failed_operation);
}

TEST(worker_pool, a_worker_cannot_wait_for_its_own_group_or_join_its_pool) {
    CE::Engine::WorkerPool pool;
    auto group = pool.make_group();
    auto result = group.submit([&] {
        EXPECT_THROW(group.drain(), CE::Exceptions::failed_operation);
        EXPECT_THROW(pool.shutdown(), CE::Exceptions::failed_operation);
    });
    result.get();
    group.close();
    group.drain();
}

TEST(worker_pool, dropping_a_group_handle_does_not_cancel_accepted_work) {
    CE::Engine::WorkerPool pool;
    std::future<int> result;
    {
        auto temporary_group = pool.make_group();
        result = temporary_group.submit([] { return 13; });
    }
    pool.shutdown();
    EXPECT_EQ(result.get(), 13);
}

TEST(worker_pool, required_unavailable_topology_rejects_instead_of_silently_falling_back) {
    CE::Engine::WorkerPool pool;
    CE::Engine::WorkerGroupOptions options;
    options.cpu.strength = CE::Engine::WorkerPolicyStrength::Required;
    options.cpu.shared_cache_domain = 1;
    EXPECT_THROW(static_cast<void>(pool.make_group(options)), CE::Exceptions::failed_operation);
}

TEST(worker_pool, effective_policy_reports_requested_shares_caps_and_available_cpu_constraints) {
    CE::Engine::WorkerPool pool(2);
    CE::Engine::WorkerGroupOptions options;
    options.max_concurrency = 1;
    options.weight = 3;
    options.priority = 2;
    const auto group = pool.make_group(options);
    EXPECT_EQ(group.policy().requested.max_concurrency, 1u);
    EXPECT_EQ(group.policy().requested.weight, 3u);
    EXPECT_EQ(group.policy().requested.priority, 2u);
    EXPECT_EQ(group.policy().effective_cpus, pool.capabilities().available_cpus);
    options.weight = 0;
    EXPECT_THROW(static_cast<void>(pool.make_group(options)), CE::Exceptions::invalid_args);
}

#if defined(__linux__)
TEST(worker_pool, a_required_cpu_group_runs_on_its_eligible_cpu) {
    CE::Engine::WorkerPool pool;
    const auto capabilities = pool.capabilities();
    if (!capabilities.cpu_affinity)
        GTEST_SKIP() << "Inherited CPU affinity cannot be queried";
    ASSERT_FALSE(capabilities.available_cpus.empty());
    CE::Engine::WorkerGroupOptions options;
    options.cpu.cpus = {capabilities.available_cpus.front()};
    options.cpu.strength = CE::Engine::WorkerPolicyStrength::Required;
    auto pinned = pool.make_group(options);
    auto selected_cpu = pinned.submit([] { return sched_getcpu(); });
    EXPECT_EQ(selected_cpu.get(), static_cast<int>(options.cpu.cpus.front()));
    pinned.close();
    pinned.drain();
}
#endif
