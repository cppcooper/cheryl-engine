#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <gtest/gtest.h>

#include <core/engine/worker-pool.h>
#include <core/engine/worker-pool-internal.h>
#include <core/engine/worker-affinity.h>
#include <internals/exceptions.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <exception>
#include <filesystem>
#include <future>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace {
    // One physical worker exercises the production policy loop. Mask values and
    // failures are synthetic; the fixture never changes the host's CPU policy.
    struct RecordingWorkerNative {
        std::vector<unsigned int> mask{2, 7};
        std::vector<std::vector<unsigned int>> sets;
        std::vector<int> failed_sets;
        int queries = 0;
        int failed_query = 0;
        bool ignore_sets = false;

        CE::Engine::WorkerDetail::WorkerNativeAdapter adapter() {
            return {true,
                [this] {
                    if (++queries == failed_query)
                        throw CE::Exceptions::failed_operation(CE_HERE, "Controlled affinity query failure");
                    return mask;
                },
                [this](const std::vector<unsigned int>& requested) {
                    sets.push_back(requested);
                    const auto attempt = static_cast<int>(sets.size());
                    if (std::find(failed_sets.begin(), failed_sets.end(), attempt) != failed_sets.end())
                        throw CE::Exceptions::failed_operation(CE_HERE, "Controlled affinity set failure " + std::to_string(attempt));
                    if (!ignore_sets)
                        mask = requested;
                },
                [](std::function<void()> work) { return std::thread(std::move(work)); }};
        }
    };

    CE::Engine::WorkerGroupOptions recording_cpu_policy(CE::Engine::WorkerPolicyStrength strength) {
        CE::Engine::WorkerGroupOptions options;
        options.cpu.cpus = {2};
        options.cpu.strength = strength;
        return options;
    }

    // Declare after the captured recording state so exceptional test exits join
    // the held worker before destroying anything its queued jobs may reference.
    class HeldWorkerCleanup final {
        CE::Engine::WorkerPool& pool_;
        std::promise<void>& release_;
        bool released_ = false;

    public:
        HeldWorkerCleanup(CE::Engine::WorkerPool& pool, std::promise<void>& release)
        : pool_(pool), release_(release) {}
        ~HeldWorkerCleanup() { release_and_join(); }
        void release_and_join() {
            if (!released_) {
                release_.set_value();
                released_ = true;
            }
            pool_.shutdown();
        }
    };
}

#if defined(__linux__)
#include <sched.h>

namespace {
    class RestoreCallingThreadAffinity final {
        const CE::Engine::WorkerDetail::WorkerNativeAdapter native_ = CE::Engine::WorkerDetail::native_worker_adapter();
        const std::vector<unsigned int> inherited_ = native_.query_affinity();

    public:
        ~RestoreCallingThreadAffinity() {
            try {
                CE::Engine::WorkerDetail::apply_affinity(native_, inherited_);
            } catch (const std::exception& error) {
                ADD_FAILURE() << "Restoring the calling test thread's affinity failed: " << error.what();
            }
        }
        RestoreCallingThreadAffinity() = default;
        RestoreCallingThreadAffinity(const RestoreCallingThreadAffinity&) = delete;
        RestoreCallingThreadAffinity& operator=(const RestoreCallingThreadAffinity&) = delete;
    };

    std::vector<unsigned int> read_worker_cpu_mask() {
        cpu_set_t mask;
        CPU_ZERO(&mask);
        if (sched_getaffinity(0, sizeof(mask), &mask) != 0)
            throw std::runtime_error("Cannot read the executing worker's CPU mask");
        std::vector<unsigned int> cpus;
        for (unsigned int cpu = 0; cpu < CPU_SETSIZE; ++cpu)
            if (CPU_ISSET(cpu, &mask))
                cpus.push_back(cpu);
        return cpus;
    }
} // namespace
#endif

TEST(worker_pool, job_results_and_failures) {
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
    const auto status = group.status();
    EXPECT_EQ(status.callback_failures, 1u);
    EXPECT_EQ(status.policy_failures, 0u);
    EXPECT_EQ(status.pool, pool.diagnostic_id());
    EXPECT_NE(status.domain, status.pool);
    EXPECT_NE(status.domain, 0u);
    EXPECT_GE(status.peak_pending, 1u);
}

TEST(worker_pool, serial_group_fifo) {
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

TEST(worker_pool, parallel_group_cap) {
    CE::Engine::WorkerPool pool(3);
    auto capped = pool.make_group({2});
    auto other = pool.make_group();
    std::promise<void> two_entered;
    std::promise<void> release;
    auto may_finish = release.get_future().share();
    std::atomic<int> entered{0};
    std::atomic<int> active{0};
    std::atomic<int> peak{0};
    std::vector<std::future<void>> jobs;
    for (int i = 0; i < 3; ++i) {
        jobs.push_back(capped.submit([&] {
            const auto running = ++active;
            auto observed = peak.load();
            while (observed < running && !peak.compare_exchange_weak(observed, running)) {}
            if (++entered == 2)
                two_entered.set_value();
            may_finish.wait();
            --active;
        }));
    }
    two_entered.get_future().wait();
    auto independent = other.submit([] { return 17; });
    // A timeout bounds failure cleanup; the two held jobs establish the state.
    EXPECT_EQ(independent.wait_for(std::chrono::seconds{1}), std::future_status::ready);
    EXPECT_EQ(capped.status().running, 2u);
    EXPECT_EQ(capped.status().pending, 1u);
    EXPECT_EQ(entered.load(), 2);
    release.set_value();
    capped.close();
    capped.drain();
    for (auto& job : jobs)
        job.get();
    EXPECT_EQ(independent.get(), 17);
    EXPECT_EQ(entered.load(), 3);
    EXPECT_EQ(peak.load(), 2);
}

TEST(worker_pool, shutdown_drain) {
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

TEST(worker_pool, worker_self_wait) {
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

TEST(worker_pool, dropped_group_handle) {
    CE::Engine::WorkerPool pool;
    std::future<int> result;
    {
        auto temporary_group = pool.make_group();
        result = temporary_group.submit([] { return 13; });
    }
    pool.shutdown();
    EXPECT_EQ(result.get(), 13);
}

TEST(worker_pool, capture_release_reentry) {
    CE::Engine::WorkerPool pool;
    auto source = pool.make_group();
    auto other = pool.make_group();
    std::future<int> follow_up;
    std::exception_ptr release_failure;
    bool released = false;
    auto resource = std::shared_ptr<int>(new int{42}, [&](int* value) noexcept {
        // Final capture release still belongs to the running job, but must not
        // hold the scheduler lock when inspecting or posting to this pool.
        const auto status = source.status();
        EXPECT_EQ(status.running, 1u);
        EXPECT_EQ(status.completed, 0u);
        try {
            follow_up = other.submit([] { return 17; });
        } catch (...) {
            release_failure = std::current_exception();
        }
        released = true;
        delete value;
    });
    auto result = source.submit([resource = std::move(resource)] { return *resource; });
    source.close();
    source.drain(); // Includes capture destruction, even after the result is ready.
    EXPECT_TRUE(released);
    EXPECT_FALSE(release_failure);
    EXPECT_EQ(result.get(), 42);
    EXPECT_EQ(source.status().completed, 1u);
    other.close();
    other.drain();
    ASSERT_TRUE(follow_up.valid());
    EXPECT_EQ(follow_up.get(), 17);
}

TEST(worker_pool, unavailable_topology) {
    CE::Engine::WorkerPool pool;
    CE::Engine::WorkerGroupOptions options;
    options.cpu.strength = CE::Engine::WorkerPolicyStrength::Required;
    options.cpu.shared_cache_domain = 1;
    EXPECT_THROW(static_cast<void>(pool.make_group(options)), CE::Exceptions::failed_operation);
}

TEST(worker_pool, effective_policy) {
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
    EXPECT_FALSE(group.policy().preferred_fallback);
    options.weight = 0;
    EXPECT_THROW(static_cast<void>(pool.make_group(options)), CE::Exceptions::invalid_args);
}

#if defined(__linux__)
TEST(worker_pool_native, inherited_cpu_mask) {
    const auto native = CE::Engine::WorkerDetail::native_worker_adapter();
    const auto inherited = native.query_affinity();
    if (inherited.size() < 2)
        GTEST_SKIP() << "Restriction/eligibility comparison requires two inherited CPUs";
    const std::vector<unsigned int> restricted{inherited.front()};
    {
        // Only this caller changes its own mask. Threads created below inherit
        // it; the guard restores it even after an exception or fatal assertion.
        RestoreCallingThreadAffinity restore;
        CE::Engine::WorkerDetail::apply_affinity(native, restricted);
        CE::Engine::WorkerPool pool;
        EXPECT_TRUE(pool.capabilities().cpu_affinity);
        EXPECT_EQ(pool.capabilities().available_cpus, restricted);
        CE::Engine::WorkerGroupOptions options;
        options.cpu.strength = CE::Engine::WorkerPolicyStrength::Required;
        options.cpu.cpus = {inherited[1]};
        EXPECT_THROW(static_cast<void>(pool.make_group(options)), CE::Exceptions::invalid_args);
        options.cpu.cpus = restricted;
        auto required = pool.make_group(options);
        options.cpu.strength = CE::Engine::WorkerPolicyStrength::Preferred;
        options.cpu.cpus = {inherited[0], inherited[1]};
        auto intersected = pool.make_group(options);
        options.cpu.cpus = {inherited[1]};
        auto fallback = pool.make_group(options);
        EXPECT_EQ(required.policy().effective_cpus, restricted);
        EXPECT_EQ(intersected.policy().effective_cpus, restricted);
        EXPECT_EQ(fallback.policy().effective_cpus, restricted);
        EXPECT_NE(intersected.policy().limitations.find("fell back"), std::string::npos);
        EXPECT_NE(fallback.policy().limitations.find("fell back"), std::string::npos);
        for (const auto* group : {&required, &intersected, &fallback}) {
            auto observation = group->submit([] { return std::pair{read_worker_cpu_mask(), sched_getcpu()}; });
            group->close();
            group->drain();
            const auto [mask, cpu] = observation.get();
            EXPECT_EQ(mask, restricted);
            EXPECT_EQ(cpu, static_cast<int>(restricted.front()));
            EXPECT_EQ(group->status().accepted, 1u);
            EXPECT_EQ(group->status().completed, 1u);
            EXPECT_EQ(group->status().policy_failures, 0u);
        }
        pool.shutdown(); // Every restricted child is joined before caller restoration.
        EXPECT_EQ(native.query_affinity(), restricted);
    }
    EXPECT_EQ(native.query_affinity(), inherited);
    ::testing::Test::RecordProperty("inherited_cpu_count", static_cast<int>(inherited.size()));
    ::testing::Test::RecordProperty("restricted_cpu", static_cast<int>(restricted.front()));
    ::testing::Test::RecordProperty("calling_mask_restored", "true");
}

TEST(worker_pool_native, native_affinity_rejection) {
    if (!std::filesystem::exists("/sys/devices/system/cpu/possible"))
        GTEST_SKIP() << "Kernel rejection acceptance requires an exposed CPU inventory";
    std::optional<unsigned int> absent_cpu;
    for (unsigned int cpu = CPU_SETSIZE; cpu-- > 0;) {
        if (!std::filesystem::exists("/sys/devices/system/cpu/cpu" + std::to_string(cpu))) {
            absent_cpu = cpu;
            break;
        }
    }
    if (!absent_cpu)
        GTEST_SKIP() << "No absent CPU ID inside the native mask is available for kernel rejection";
    CE::Engine::WorkerPool pool;
    const auto inherited = pool.capabilities().available_cpus;
    CE::Engine::WorkerGroupOptions options;
    options.cpu.strength = CE::Engine::WorkerPolicyStrength::Required;
    auto group = pool.make_group(options);
    bool entered = false;
    auto rejected = group.submit([cpu = *absent_cpu, &entered] {
        // The production adapter reaches pthread_setaffinity_np with a nonempty
        // mask and an in-range ID. The kernel rejects it after callback entry.
        entered = true;
        const auto native = CE::Engine::WorkerDetail::native_worker_adapter();
        native.set_affinity({cpu});
    });
    std::string rejection_message;
    try {
        rejected.get();
        ADD_FAILURE() << "The kernel accepted an absent CPU's affinity mask";
    } catch (const CE::Exceptions::failed_operation& error) {
        rejection_message = error.what();
        EXPECT_NE(rejection_message.find("Setting worker CPU affinity failed"), std::string::npos);
    }
    auto recovery = group.submit([] { return read_worker_cpu_mask(); });
    group.close();
    group.drain();
    EXPECT_TRUE(entered);
    EXPECT_EQ(recovery.get(), inherited);
    EXPECT_EQ(group.status().accepted, 2u);
    EXPECT_EQ(group.status().completed, 2u);
    EXPECT_EQ(group.status().policy_failures, 0u); // Failure occurred inside the callback.
    EXPECT_EQ(group.status().running, 0u);
    EXPECT_EQ(group.status().pending, 0u);
    ::testing::Test::RecordProperty("kernel_rejected_cpu", static_cast<int>(*absent_cpu));
    ::testing::Test::RecordProperty("kernel_rejection", rejection_message);
}

TEST(worker_pool, required_cpu_affinity) {
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

TEST(worker_pool, group_mask_restoration) {
    CE::Engine::WorkerPool pool;
    const auto capabilities = pool.capabilities();
    if (!capabilities.cpu_affinity || capabilities.available_cpus.size() < 2)
        GTEST_SKIP() << "Mask restoration requires at least two eligible CPUs";
    CE::Engine::WorkerGroupOptions options;
    options.cpu.strength = CE::Engine::WorkerPolicyStrength::Required;
    options.cpu.cpus = {capabilities.available_cpus.front()};
    auto pinned = pool.make_group(options);
    options.cpu.cpus.clear();
    auto inherited = pool.make_group(options);
    const std::vector<unsigned int> pinned_mask{capabilities.available_cpus.front()};
    EXPECT_EQ(pinned.submit(read_worker_cpu_mask).get(), pinned_mask);
    EXPECT_EQ(inherited.submit(read_worker_cpu_mask).get(), capabilities.available_cpus);
    EXPECT_EQ(pinned.submit(read_worker_cpu_mask).get(), pinned_mask);
    pinned.close();
    inherited.close();
    pinned.drain();
    inherited.drain();
}

TEST(worker_pool, cached_mask_revalidation) {
    CE::Engine::WorkerPool pool;
    const auto capabilities = pool.capabilities();
    if (!capabilities.cpu_affinity || capabilities.available_cpus.size() < 2)
        GTEST_SKIP() << "Cached-mask revalidation requires at least two eligible CPUs";
    CE::Engine::WorkerGroupOptions options;
    options.cpu.strength = CE::Engine::WorkerPolicyStrength::Required;
    auto inherited = pool.make_group(options);
    const auto first_cpu = capabilities.available_cpus.front();
    // The first job changes native state behind the adapter's verified-mask cache.
    auto narrowed = inherited.submit([first_cpu] {
        cpu_set_t mask;
        CPU_ZERO(&mask);
        CPU_SET(first_cpu, &mask);
        if (sched_setaffinity(0, sizeof(mask), &mask) != 0)
            throw std::runtime_error("Cannot narrow the executing worker's CPU mask");
        return read_worker_cpu_mask();
    });
    EXPECT_EQ(narrowed.get(), (std::vector<unsigned int>{first_cpu}));
    // The same group must verify native state and restore its full effective set.
    EXPECT_EQ(inherited.submit(read_worker_cpu_mask).get(), capabilities.available_cpus);
    inherited.close();
    inherited.drain();
}

TEST(worker_pool, overlapping_cpu_groups) {
    CE::Engine::WorkerPool pool;
    const auto capabilities = pool.capabilities();
    if (!capabilities.cpu_affinity || capabilities.available_cpus.size() < 2)
        GTEST_SKIP() << "Overlapping CPU groups require at least two eligible CPUs";
    CE::Engine::WorkerGroupOptions options;
    options.weight = 3;
    options.cpu.strength = CE::Engine::WorkerPolicyStrength::Required;
    options.cpu.cpus = {capabilities.available_cpus[0]};
    auto frequent = pool.make_group(options);
    const auto frequent_mask = frequent.policy().effective_cpus;
    options.weight = 1;
    options.cpu.cpus.push_back(capabilities.available_cpus[1]);
    auto regular = pool.make_group(options);
    const auto regular_mask = regular.policy().effective_cpus;

    auto gate = pool.make_group();
    std::promise<void> entered;
    std::promise<void> release;
    auto released = release.get_future().share();
    std::vector<char> order;
    std::vector<std::future<std::vector<unsigned int>>> jobs;
    HeldWorkerCleanup cleanup(pool, release);
    auto blocked = gate.submit([&] {
        entered.set_value();
        released.wait();
    });
    entered.get_future().wait();
    // Queue both workloads while the sole physical worker is held. Each job
    // records its actual native mask before the next group's policy is applied.
    for (int i = 0; i < 12; ++i)
        jobs.push_back(frequent.submit([&] {
            order.push_back('F');
            return read_worker_cpu_mask();
        }));
    for (int i = 0; i < 4; ++i)
        jobs.push_back(regular.submit([&] {
            order.push_back('R');
            return read_worker_cpu_mask();
        }));
    cleanup.release_and_join();
    blocked.get();
    for (std::size_t i = 0; i < jobs.size(); ++i)
        EXPECT_EQ(jobs[i].get(), i < 12 ? frequent_mask : regular_mask);
    ASSERT_EQ(order.size(), 16u);
    EXPECT_EQ(std::count(order.begin(), order.begin() + 8, 'F'), 6);
    EXPECT_EQ(std::count(order.begin(), order.begin() + 8, 'R'), 2);
}
#endif

TEST(worker_pool, weighted_fairness) {
    CE::Engine::WorkerPool pool;
    auto gate = pool.make_group();
    std::promise<void> entered;
    std::promise<void> release;
    auto released = release.get_future().share();
    std::vector<char> order;
    std::vector<std::future<void>> jobs;
    HeldWorkerCleanup cleanup(pool, release);
    auto blocked = gate.submit([&] {
        entered.set_value();
        released.wait();
    });
    entered.get_future().wait();
    CE::Engine::WorkerGroupOptions options;
    options.weight = 3;
    auto frequent = pool.make_group(options);
    auto regular = pool.make_group();
    for (int i = 0; i < 12; ++i)
        jobs.push_back(frequent.submit([&] { order.push_back('F'); }));
    for (int i = 0; i < 4; ++i)
        jobs.push_back(regular.submit([&] { order.push_back('R'); }));
    cleanup.release_and_join();
    blocked.get();
    for (auto& job : jobs)
        job.get();
    ASSERT_EQ(order.size(), 16u);
    EXPECT_EQ(std::count(order.begin(), order.begin() + 8, 'F'), 6);
    EXPECT_EQ(std::count(order.begin(), order.begin() + 8, 'R'), 2);
}

TEST(worker_pool_faults, discovery_query_failure) {
    RecordingWorkerNative native;
    native.failed_query = 1;
    auto pool = CE::Engine::WorkerDetail::WorkerPoolAccess::create(1, native.adapter());
    EXPECT_FALSE(pool->capabilities().cpu_affinity);
    EXPECT_TRUE(pool->capabilities().available_cpus.empty());
    EXPECT_THROW(
        static_cast<void>(pool->make_group(recording_cpu_policy(CE::Engine::WorkerPolicyStrength::Required))),
        CE::Exceptions::failed_operation
    );
    auto ordinary = pool->make_group();
    auto result = ordinary.submit([] { return 13; });
    ordinary.close();
    ordinary.drain();
    EXPECT_EQ(result.get(), 13);
    EXPECT_TRUE(native.sets.empty());
    EXPECT_EQ(ordinary.status().completed, 1u);
}

TEST(worker_pool_faults, required_readback_mismatch) {
    RecordingWorkerNative native;
    native.ignore_sets = true;
    auto pool = CE::Engine::WorkerDetail::WorkerPoolAccess::create(1, native.adapter());
    auto group = pool->make_group(recording_cpu_policy(CE::Engine::WorkerPolicyStrength::Required));
    int calls = 0;
    auto result = group.submit([&] { ++calls; });
    group.close();
    group.drain();
    EXPECT_THROW(result.get(), CE::Exceptions::failed_operation);
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(native.sets, (std::vector<std::vector<unsigned int>>{{2}}));
    EXPECT_EQ(group.status().policy_failures, 1u);
    EXPECT_EQ(group.status().completed, 1u);
}

TEST(worker_pool_faults, required_affinity_failure) {
    RecordingWorkerNative native;
    native.failed_sets = {1};
    auto pool = CE::Engine::WorkerDetail::WorkerPoolAccess::create(1, native.adapter());
    auto required = pool->make_group(recording_cpu_policy(CE::Engine::WorkerPolicyStrength::Required));
    auto ordinary = pool->make_group();
    int calls = 0;
    bool released = false;
    std::future<int> follow_up;
    std::exception_ptr release_failure;
    auto capture = std::shared_ptr<int>(new int{42}, [&](int* value) noexcept {
        EXPECT_EQ(required.status().running, 1u);
        EXPECT_EQ(required.status().completed, 0u);
        try {
            follow_up = ordinary.submit([] { return 17; });
        } catch (...) {
            release_failure = std::current_exception();
        }
        released = true;
        delete value;
    });
    auto result = required.submit([capture = std::move(capture), &calls] {
        ++calls;
        return *capture;
    });
    required.close();
    required.drain();
    EXPECT_THROW(result.get(), CE::Exceptions::failed_operation);
    EXPECT_EQ(calls, 0);
    EXPECT_TRUE(released);
    EXPECT_FALSE(release_failure);
    EXPECT_EQ(required.status().policy_failures, 1u);
    EXPECT_EQ(required.status().completed, 1u);
    ordinary.close();
    ordinary.drain();
    ASSERT_TRUE(follow_up.valid());
    EXPECT_EQ(follow_up.get(), 17);
}

TEST(worker_pool_faults, preferred_affinity_fallback) {
    RecordingWorkerNative native;
    native.failed_sets = {1};
    auto pool = CE::Engine::WorkerDetail::WorkerPoolAccess::create(1, native.adapter());
    auto group = pool->make_group(recording_cpu_policy(CE::Engine::WorkerPolicyStrength::Preferred));
    auto result = group.submit([&] { return native.mask; });
    group.close();
    group.drain();
    EXPECT_EQ(result.get(), (std::vector<unsigned int>{2, 7}));
    EXPECT_EQ(native.sets, (std::vector<std::vector<unsigned int>>{{2}, {2, 7}}));
    EXPECT_EQ(group.status().policy_failures, 1u);
    EXPECT_EQ(group.status().completed, 1u);
}

TEST(worker_pool_faults, preferred_fallback_failure) {
    for (const bool query_failure : {false, true}) {
        RecordingWorkerNative native;
        native.failed_sets = query_failure ? std::vector<int>{1} : std::vector<int>{1, 2};
        native.failed_query = query_failure ? 2 : 0;
        auto pool = CE::Engine::WorkerDetail::WorkerPoolAccess::create(1, native.adapter());
        auto preferred = pool->make_group(recording_cpu_policy(CE::Engine::WorkerPolicyStrength::Preferred));
        auto ordinary = pool->make_group();
        int calls = 0;
        auto failure = preferred.submit([&] { ++calls; });
        preferred.close();
        preferred.drain();
        try {
            failure.get();
            ADD_FAILURE() << "Both native attempts failed but the job succeeded";
        } catch (const CE::Exceptions::failed_operation& error) {
            EXPECT_NE(std::string(error.what()).find(query_failure ? "query failure" : "set failure 2"), std::string::npos);
        }
        auto recovery = ordinary.submit([&] { return native.mask; });
        ordinary.close();
        ordinary.drain();
        EXPECT_EQ(recovery.get(), (std::vector<unsigned int>{2, 7}));
        EXPECT_EQ(calls, 0);
        EXPECT_EQ(native.sets, (std::vector<std::vector<unsigned int>>{{2}, {2, 7}, {2, 7}}));
        EXPECT_EQ(preferred.status().policy_failures, 1u);
        EXPECT_EQ(ordinary.status().policy_failures, 0u);
    }
}

TEST(worker_pool_faults, post_set_query_failure) {
    RecordingWorkerNative native;
    native.failed_query = 2; // The set succeeds, but its readback cannot verify it.
    auto pool = CE::Engine::WorkerDetail::WorkerPoolAccess::create(1, native.adapter());
    auto required = pool->make_group(recording_cpu_policy(CE::Engine::WorkerPolicyStrength::Required));
    auto ordinary = pool->make_group();
    int calls = 0;
    auto failure = required.submit([&] { ++calls; });
    required.close();
    required.drain();
    EXPECT_THROW(failure.get(), CE::Exceptions::failed_operation);
    auto recovery = ordinary.submit([&] { return native.mask; });
    ordinary.close();
    ordinary.drain();
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(recovery.get(), (std::vector<unsigned int>{2, 7}));
    EXPECT_EQ(native.sets, (std::vector<std::vector<unsigned int>>{{2}, {2, 7}}));
    EXPECT_EQ(required.status().policy_failures, 1u);
}

TEST(worker_pool_faults, cached_mask_query_failure) {
    RecordingWorkerNative native;
    native.failed_query = 2; // Discovery succeeds; the cached required mask read fails.
    auto pool = CE::Engine::WorkerDetail::WorkerPoolAccess::create(1, native.adapter());
    CE::Engine::WorkerGroupOptions options;
    options.cpu.strength = CE::Engine::WorkerPolicyStrength::Required;
    auto group = pool->make_group(options);
    int calls = 0;
    auto failure = group.submit([&] { ++calls; });
    EXPECT_THROW(failure.get(), CE::Exceptions::failed_operation);
    auto recovery = group.submit([&] { return native.mask; });
    group.close();
    group.drain();
    EXPECT_EQ(recovery.get(), (std::vector<unsigned int>{2, 7}));
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(native.sets, (std::vector<std::vector<unsigned int>>{{2, 7}}));
    EXPECT_EQ(group.status().policy_failures, 1u);
    EXPECT_EQ(group.status().completed, 2u);
}

TEST(worker_pool_faults, partial_thread_start) {
    std::promise<void> entered;
    auto started = entered.get_future().share();
    std::atomic<int> exited{0};
    int attempts = 0;
    auto captured = std::make_shared<int>(42);
    std::weak_ptr<int> retained = captured;
    CE::Engine::WorkerDetail::WorkerNativeAdapter adapter;
    adapter.start_thread = [captured = std::move(captured), &attempts, &entered, started, &exited](std::function<void()> work) {
        if (++attempts == 2) {
            started.wait(); // The first thread has actually entered its wrapper.
            throw CE::Exceptions::failed_operation(CE_HERE, "Controlled second thread-start failure");
        }
        return std::thread([work = std::move(work), &entered, &exited] {
            entered.set_value();
            work(); // Constructor rollback must close/wake this idle worker.
            ++exited;
        });
    };
    try {
        static_cast<void>(CE::Engine::WorkerDetail::WorkerPoolAccess::create(3, std::move(adapter)));
        ADD_FAILURE() << "The second thread start did not reject construction";
    } catch (const CE::Exceptions::failed_operation& error) {
        EXPECT_NE(std::string(error.what()).find("second thread-start failure"), std::string::npos);
    }
    EXPECT_EQ(attempts, 2);
    EXPECT_EQ(exited.load(), 1); // Joined before the failing constructor returns.
    // A moved-from std::function may retain source ownership; only failed-pool
    // and started-thread ownership must be gone at this boundary.
    adapter = {};
    EXPECT_TRUE(retained.expired());
}
