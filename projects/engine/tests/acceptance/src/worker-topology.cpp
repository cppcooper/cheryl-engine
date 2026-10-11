#include <core/engine/worker-pool.h>
#include <core/engine/worker-pool-internal.h>
#include <internals/exceptions.h>
#include <testing/worker-topology.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <vector>

namespace {
    namespace Engine = CE::Engine;

    Engine::WorkerGroupOptions locality_options(Engine::WorkerLocalityPolicy locality, const std::size_t concurrency = 0) {
        Engine::WorkerGroupOptions options;
        options.max_concurrency = concurrency;
        options.cpu.locality = locality;
        return options;
    }

    unsigned int core_for(const unsigned int cpu) {
        for (const auto& core : CE::Testing::sparse_worker_topology().cores)
            if (std::ranges::find(core.cpus, cpu) != core.cpus.end())
                return core.id;
        throw std::logic_error("Unexpected fixture CPU");
    }
}

TEST(worker_locality, sizing) {
    auto provider = std::make_shared<CE::Testing::WorkerTopologyProvider>();
    {
        Engine::WorkerPool pool(Engine::WorkerPoolOptions{0, provider});
        EXPECT_EQ(pool.worker_count(), 3u);
        EXPECT_TRUE(pool.capabilities().core_topology);
        EXPECT_TRUE(pool.capabilities().cache_topology);
        EXPECT_FALSE(pool.capabilities().numa_placement);
        const auto group = pool.make_group();
        EXPECT_EQ(group.policy().effective_max_concurrency, 3u);
        EXPECT_EQ(group.policy().effective_locality, Engine::WorkerLocalityPolicy::Automatic);
    }
    provider->snapshot.cores.pop_back();
    {
        Engine::WorkerPool pool(Engine::WorkerPoolOptions{0, provider});
        EXPECT_EQ(pool.worker_count(), 6u);
        EXPECT_TRUE(pool.make_group().policy().preferred_fallback);
    }
    provider->fail_query = true;
    {
        Engine::WorkerPool pool(Engine::WorkerPoolOptions{0, provider});
        EXPECT_EQ(pool.worker_count(), 1u);
        EXPECT_EQ(pool.make_group().submit([] { return 7; }).get(), 7);
    }
    Engine::WorkerPool ordinary;
    EXPECT_EQ(ordinary.worker_count(), 1u);
    EXPECT_FALSE(ordinary.make_group().policy().effective_locality);
    EXPECT_THROW(Engine::WorkerPool{0}, CE::Exceptions::invalid_args);
}

TEST(worker_locality, owned_snapshot) {
    auto provider = std::make_shared<CE::Testing::WorkerTopologyProvider>();
    provider->inherited = {17, 2000};
    Engine::WorkerPool pool(Engine::WorkerPoolOptions{0, provider});
    provider->snapshot.cores.clear();
    provider->snapshot.caches.clear();
    auto snapshot = pool.topology();
    EXPECT_EQ(snapshot.eligible_cpus, provider->inherited);
    EXPECT_EQ(snapshot.cores.size(), 2u);
    EXPECT_EQ(snapshot.caches.size(), 4u);
    snapshot.cores.clear();
    EXPECT_EQ(pool.topology().cores.size(), 2u);
    auto group = pool.make_group();
    for (int index = 0; index < 4; ++index)
        EXPECT_EQ(group.submit([&] { return provider->current_thread_cpus().size(); }).get(), 1u);
    EXPECT_EQ(provider->discoveries, 1u);
}

TEST(worker_locality, cache_policy) {
    auto provider = std::make_shared<CE::Testing::WorkerTopologyProvider>();
    Engine::WorkerPool pool(Engine::WorkerPoolOptions{2, provider});
    auto options = locality_options(Engine::WorkerLocalityPolicy::Reuse, 9);
    options.cpu.strength = Engine::WorkerPolicyStrength::Required;
    options.cpu.cpus = {2000, 17};
    options.cpu.shared_cache_domain = 52; // Private instruction cache of core 7.
    auto group = pool.make_group(options);
    EXPECT_EQ(group.policy().effective_cpus, (std::vector<unsigned int>{2000}));
    EXPECT_EQ(group.policy().effective_cache_domain, 52u);
    EXPECT_EQ(group.policy().effective_max_concurrency, 2u);
    EXPECT_FALSE(group.policy().preferred_fallback);
    EXPECT_EQ(group.submit([&] { return provider->current_thread_cpus(); }).get(), (std::vector<unsigned int>{2000}));
    EXPECT_EQ(group.status().last_cpu, 2000u);
    options.cpu.cpus = {17};
    EXPECT_THROW(static_cast<void>(pool.make_group(options)), CE::Exceptions::invalid_args);
    options.cpu.shared_cache_domain = 1234;
    EXPECT_THROW(static_cast<void>(pool.make_group(options)), CE::Exceptions::invalid_args);
    options.cpu.shared_cache_domain.reset();
    options.cpu.cpus = {18};
    EXPECT_THROW(static_cast<void>(pool.make_group(options)), CE::Exceptions::invalid_args);
    options.cpu.strength = Engine::WorkerPolicyStrength::Preferred;
    options.cpu.cpus = {2, 18};
    auto fallback = pool.make_group(options);
    EXPECT_EQ(fallback.policy().effective_cpus, (std::vector<unsigned int>{2}));
    EXPECT_TRUE(fallback.policy().preferred_fallback);
    options.cpu.shared_cache_domain = 100;
    fallback = pool.make_group(options);
    EXPECT_FALSE(fallback.policy().effective_cache_domain);
    EXPECT_EQ(fallback.policy().effective_cpus, (std::vector<unsigned int>{2}));
}

TEST(worker_locality, unavailable_policy) {
    auto provider = std::make_shared<CE::Testing::WorkerTopologyProvider>();
    provider->snapshot.cores.clear();
    provider->snapshot.caches.clear();
    Engine::WorkerPool pool(Engine::WorkerPoolOptions{1, provider});
    auto options = locality_options(Engine::WorkerLocalityPolicy::Spread);
    options.cpu.strength = Engine::WorkerPolicyStrength::Required;
    EXPECT_THROW(static_cast<void>(pool.make_group(options)), CE::Exceptions::failed_operation);
    options.cpu.locality.reset();
    options.cpu.shared_cache_domain = 51;
    EXPECT_THROW(static_cast<void>(pool.make_group(options)), CE::Exceptions::failed_operation);
    options.cpu.shared_cache_domain.reset();
    options.cpu.cpus = {4097};
    EXPECT_EQ(pool.make_group(options).submit([&] { return provider->current_thread_cpus(); }).get(), (std::vector<unsigned int>{4097}));
    options.cpu.strength = Engine::WorkerPolicyStrength::Preferred;
    options.cpu.locality = Engine::WorkerLocalityPolicy::Spread;
    const auto fallback = pool.make_group(options);
    EXPECT_TRUE(fallback.policy().preferred_fallback);
    EXPECT_FALSE(fallback.policy().core_topology);
    EXPECT_EQ(fallback.policy().effective_locality, Engine::WorkerLocalityPolicy::Spread);

    provider->snapshot.cpu_affinity = false;
    Engine::WorkerPool unbound(Engine::WorkerPoolOptions{1, provider});
    EXPECT_FALSE(unbound.make_group(options).policy().effective_locality);
    options.cpu.strength = Engine::WorkerPolicyStrength::Required;
    EXPECT_THROW(static_cast<void>(unbound.make_group(options)), CE::Exceptions::failed_operation);
    Engine::WorkerPool ordinary;
    options.cpu.cpus.clear();
    EXPECT_THROW(static_cast<void>(ordinary.make_group(options)), CE::Exceptions::failed_operation);
}

TEST(worker_locality, reuse) {
    for (const auto locality : {Engine::WorkerLocalityPolicy::Reuse, Engine::WorkerLocalityPolicy::Automatic}) {
        auto provider = std::make_shared<CE::Testing::WorkerTopologyProvider>();
        Engine::WorkerPool pool(Engine::WorkerPoolOptions{1, provider});
        auto group = pool.make_group(locality_options(locality, 1));
        std::optional<unsigned int> first;
        for (int index = 0; index < 12; ++index) {
            const auto mask = group.submit([&] { return provider->current_thread_cpus(); }).get();
            ASSERT_EQ(mask.size(), 1u);
            if (!first)
                first = mask.front();
            EXPECT_EQ(mask.front(), first);
        }
        group.close();
        group.drain();
        EXPECT_EQ(provider->bindings().size(), 1u);
        EXPECT_EQ(group.status().last_cpu, first);
        EXPECT_EQ(group.status().completed, 12u);
    }
}

TEST(worker_locality, spread) {
    auto provider = std::make_shared<CE::Testing::WorkerTopologyProvider>();
    Engine::WorkerPool pool(Engine::WorkerPoolOptions{1, provider});
    auto group = pool.make_group(locality_options(Engine::WorkerLocalityPolicy::Spread));
    std::set<unsigned int> cpus;
    for (int index = 0; index < 6; ++index) {
        const auto mask = group.submit([&] { return provider->current_thread_cpus(); }).get();
        ASSERT_EQ(mask.size(), 1u);
        cpus.insert(mask.front());
    }
    EXPECT_EQ(cpus, (std::set<unsigned int>{2, 17, 901, 2000, 4097, 5001}));
}

TEST(worker_locality, cores_before_smt) {
    for (const auto locality : {Engine::WorkerLocalityPolicy::Spread, Engine::WorkerLocalityPolicy::Automatic}) {
        auto provider = std::make_shared<CE::Testing::WorkerTopologyProvider>();
        Engine::WorkerPool pool(Engine::WorkerPoolOptions{4, provider});
        auto group = pool.make_group(locality_options(locality));
        std::mutex mutex;
        std::condition_variable entered;
        std::vector<unsigned int> cpus;
        std::vector<std::future<void>> jobs;
        CE::Testing::WorkerRelease release(pool);
        for (int index = 0; index < 3; ++index)
            jobs.push_back(group.submit([&] {
                const auto mask = provider->current_thread_cpus();
                {
                    std::lock_guard lock(mutex);
                    cpus.push_back(mask.at(0));
                }
                entered.notify_one();
                release.ready.wait();
            }));
        {
            std::unique_lock lock(mutex);
            ASSERT_TRUE(entered.wait_for(lock, std::chrono::seconds{3}, [&] { return cpus.size() == 3; }));
            std::set<unsigned int> cores;
            for (const auto cpu : cpus)
                cores.insert(core_for(cpu));
            EXPECT_EQ(cores.size(), 3u);
        }
        auto sibling = group.submit([&] { return provider->current_thread_cpus().at(0); });
        ASSERT_EQ(sibling.wait_for(std::chrono::seconds{3}), std::future_status::ready);
        EXPECT_EQ(std::ranges::find(cpus, sibling.get()), cpus.end());
        release.release();
        group.close();
        group.drain();
        for (auto& job : jobs)
            job.get();
    }
}

TEST(worker_locality, soft_progress_and_cap) {
    auto provider = std::make_shared<CE::Testing::WorkerTopologyProvider>();
    Engine::WorkerPool pool(Engine::WorkerPoolOptions{3, provider});
    auto group = pool.make_group(locality_options(Engine::WorkerLocalityPolicy::Reuse, 2));
    std::vector<std::future<void>> held;
    std::promise<unsigned int> first;
    std::promise<unsigned int> second;
    CE::Testing::WorkerRelease release(pool);
    held.push_back(group.submit([&] {
        first.set_value(provider->current_thread_cpus().at(0));
        release.ready.wait();
    }));
    const auto first_cpu = first.get_future().get();
    held.push_back(group.submit([&] {
        second.set_value(provider->current_thread_cpus().at(0));
        release.ready.wait();
    }));
    auto second_cpu = second.get_future();
    ASSERT_EQ(second_cpu.wait_for(std::chrono::seconds{3}), std::future_status::ready);
    EXPECT_NE(core_for(first_cpu), core_for(second_cpu.get()));
    auto capped = group.submit([] { return 11; });
    auto pinned_options = locality_options(Engine::WorkerLocalityPolicy::Reuse);
    pinned_options.cpu.cpus = {first_cpu};
    auto pinned = pool.make_group(pinned_options);
    auto independent = pinned.submit([&] { return provider->current_thread_cpus().at(0); });
    ASSERT_EQ(independent.wait_for(std::chrono::seconds{3}), std::future_status::ready);
    EXPECT_EQ(independent.get(), first_cpu);
    EXPECT_EQ(group.status().running, 2u);
    EXPECT_EQ(group.status().pending, 1u);
    release.release();
    group.close();
    group.drain();
    for (auto& job : held)
        job.get();
    EXPECT_EQ(capped.get(), 11);
}

TEST(worker_locality, fairness) {
    auto provider = std::make_shared<CE::Testing::WorkerTopologyProvider>();
    Engine::WorkerPool pool(Engine::WorkerPoolOptions{1, provider});
    auto blocker = pool.make_group();
    std::promise<void> entered;
    std::vector<char> order;
    CE::Testing::WorkerRelease release(pool);
    auto held = blocker.submit([&] {
        entered.set_value();
        release.ready.wait();
    });
    entered.get_future().wait();
    auto heavy_options = locality_options(Engine::WorkerLocalityPolicy::Reuse);
    heavy_options.weight = 3;
    auto heavy = pool.make_group(heavy_options);
    auto light = pool.make_group(locality_options(Engine::WorkerLocalityPolicy::Spread));
    std::vector<std::future<void>> jobs;
    for (int index = 0; index < 12; ++index) {
        jobs.push_back(heavy.submit([&] { order.push_back('h'); }));
        jobs.push_back(light.submit([&] { order.push_back('l'); }));
    }
    release.release();
    pool.shutdown();
    held.get();
    for (auto& job : jobs)
        job.get();
    ASSERT_EQ(order.size(), 24u);
    EXPECT_EQ(std::count(order.begin(), order.begin() + 8, 'h'), 6);
    EXPECT_EQ(std::count(order.begin(), order.begin() + 8, 'l'), 2);
}

TEST(worker_locality, binding_failures) {
    for (const bool readback : {false, true}) {
        auto provider = std::make_shared<CE::Testing::WorkerTopologyProvider>();
        provider->fail_singleton = !readback;
        provider->ignore_singleton = readback;
        Engine::WorkerPool pool(Engine::WorkerPoolOptions{1, provider});
        auto options = locality_options(Engine::WorkerLocalityPolicy::Reuse);
        options.cpu.strength = Engine::WorkerPolicyStrength::Required;
        auto required = pool.make_group(options);
        std::atomic<int> calls{0};
        auto rejected = required.submit([&] { ++calls; });
        required.close();
        required.drain();
        EXPECT_THROW(rejected.get(), std::exception);
        EXPECT_EQ(calls, 0);
        EXPECT_EQ(required.status().policy_failures, 1u);
        EXPECT_FALSE(required.status().last_cpu);
        options.cpu.strength = Engine::WorkerPolicyStrength::Preferred;
        auto preferred = pool.make_group(options);
        EXPECT_EQ(preferred.submit([&] { return provider->current_thread_cpus(); }).get(), provider->inherited);
        EXPECT_EQ(preferred.status().policy_failures, 1u);
        EXPECT_FALSE(preferred.status().last_cpu);
        provider->fail_binding = true;
        EXPECT_THROW(preferred.submit([&] { ++calls; }).get(), std::exception);
        EXPECT_EQ(calls, 0);
        provider->fail_binding = false;
        provider->fail_singleton = false;
        provider->ignore_singleton = false;
        EXPECT_EQ(preferred.submit([&] { return provider->current_thread_cpus().size(); }).get(), 1u);
    }
}

TEST(worker_locality, required_revalidation) {
    auto provider = std::make_shared<CE::Testing::WorkerTopologyProvider>();
    Engine::WorkerPool pool(Engine::WorkerPoolOptions{1, provider});
    auto options = locality_options(Engine::WorkerLocalityPolicy::Reuse);
    options.cpu.strength = Engine::WorkerPolicyStrength::Required;
    auto group = pool.make_group(options);
    const auto first = group.submit([&] { return provider->current_thread_cpus(); }).get();
    provider->reset_masks();
    EXPECT_EQ(group.submit([&] { return provider->current_thread_cpus(); }).get(), first);
    EXPECT_EQ(provider->bindings().size(), 2u);
    provider->fail_query = true;
    EXPECT_THROW(group.submit([] {}).get(), std::exception);
}

TEST(worker_locality, provider_lifetime) {
    auto provider = std::make_shared<CE::Testing::WorkerTopologyProvider>();
    const std::weak_ptr<Engine::WorkerTopologyProvider> lifetime = provider;
    auto pool = std::make_unique<Engine::WorkerPool>(Engine::WorkerPoolOptions{2, provider});
    auto group = pool->make_group();
    provider.reset();
    EXPECT_FALSE(lifetime.expired());
    auto capture = std::make_shared<int>(19);
    const std::weak_ptr<int> captured = capture;
    auto result = group.submit([capture = std::move(capture), lifetime] {
        if (lifetime.expired())
            throw std::logic_error("Provider destroyed before worker join");
        return *capture;
    });
    pool.reset();
    EXPECT_EQ(result.get(), 19);
    EXPECT_TRUE(captured.expired());
    EXPECT_TRUE(lifetime.expired());
    EXPECT_EQ(group.status().completed, 1u);
    EXPECT_FALSE(group.status().accepting);
}

TEST(worker_locality, startup_failure) {
    auto provider = std::make_shared<CE::Testing::WorkerTopologyProvider>();
    std::promise<void> entered;
    auto started = entered.get_future();
    std::atomic<int> exits{0};
    int attempts = 0;
    Engine::WorkerDetail::WorkerNativeAdapter adapter;
    adapter.start_thread = [&](std::function<void()> work) {
        if (++attempts == 2) {
            started.wait();
            throw std::runtime_error("Controlled startup failure");
        }
        return std::thread([&, work = std::move(work)] {
            entered.set_value();
            work();
            EXPECT_EQ(provider.use_count() > 1, true);
            ++exits;
        });
    };
    EXPECT_THROW(static_cast<void>(Engine::WorkerDetail::WorkerPoolAccess::create({3, provider}, std::move(adapter))), std::runtime_error);
    EXPECT_EQ(exits, 1);
    EXPECT_EQ(provider.use_count(), 1);
}
