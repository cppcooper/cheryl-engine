#include <core/engine/worker-configuration.h>
#include <core/engine/worker-topology-internal.h>
#include <internals/exceptions.h>
#include <testing/worker-topology.h>
#include <gtest/gtest.h>

#include <functional>
#include <limits>
#include <string>
#include <vector>

TEST(worker_topology, inherited) {
    CE::Testing::WorkerTopologyProvider provider;
    provider.inherited = {17, 2000};
    auto topology = CE::Engine::WorkerDetail::prepare_topology(provider);
    EXPECT_EQ(topology.eligible_cpus, (std::vector<unsigned int>{17, 2000}));
    ASSERT_EQ(topology.cores.size(), 2u);
    EXPECT_EQ(topology.cores[0].id, 7u);
    EXPECT_EQ(topology.cores[0].cpus, (std::vector<unsigned int>{2000}));
    ASSERT_EQ(topology.caches.size(), 4u);
    EXPECT_EQ(topology.caches[1].type, CE::Engine::WorkerCacheType::Instruction);
    EXPECT_EQ(topology.caches[1].capacity_bytes, 16384u);
    EXPECT_EQ(topology.caches[3].id, 99u);
    EXPECT_TRUE(CE::Engine::WorkerDetail::complete_core_topology(topology));
    EXPECT_EQ(provider.snapshot.eligible_cpus.size(), 6u);
}

TEST(worker_topology, invalid_inventory) {
    using Topology = CE::Engine::WorkerTopology;
    const std::vector<std::function<void(Topology&)>> invalid{[](Topology& value) { value.eligible_cpus.push_back(2); },
        [](Topology& value) { value.cores[1].id = value.cores[0].id; }, [](Topology& value) { value.cores[1].cpus.push_back(2); },
        [](Topology& value) { value.cores[0].cpus.push_back(2); }, [](Topology& value) { value.caches[1].id = value.caches[0].id; },
        [](Topology& value) { value.caches[0].level = 0; },
        [](Topology& value) { value.caches[0].type = static_cast<CE::Engine::WorkerCacheType>(99); },
        [](Topology& value) { value.caches[0].capacity_bytes = 0; }, [](Topology& value) { value.caches[0].line_bytes = 0; },
        [](Topology& value) { value.caches[0].cpus.push_back(2); }};
    for (std::size_t index = 0; index < invalid.size(); ++index) {
        SCOPED_TRACE(index);
        CE::Testing::WorkerTopologyProvider provider;
        invalid[index](provider.snapshot);
        EXPECT_THROW(static_cast<void>(CE::Engine::WorkerDetail::prepare_topology(provider)), CE::Exceptions::invalid_args);
    }
}

TEST(worker_topology, missing_information) {
    CE::Testing::WorkerTopologyProvider provider;
    provider.snapshot.cores.pop_back();
    provider.snapshot.caches.clear();
    const auto partial = CE::Engine::WorkerDetail::prepare_topology(provider);
    EXPECT_FALSE(CE::Engine::WorkerDetail::complete_core_topology(partial));
    EXPECT_NE(partial.limitations.find("incomplete"), std::string::npos);
    EXPECT_NE(partial.limitations.find("cache"), std::string::npos);
    provider.fail_query = true;
    const auto unavailable = CE::Engine::WorkerDetail::prepare_topology(provider);
    EXPECT_FALSE(unavailable.cpu_affinity);
    EXPECT_TRUE(unavailable.eligible_cpus.empty());
    EXPECT_TRUE(unavailable.cores.empty());
    EXPECT_TRUE(unavailable.caches.empty());
    EXPECT_NE(unavailable.limitations.find("inherited"), std::string::npos);
}

TEST(worker_topology, separate_cache_roots) {
    CE::Testing::WorkerTopologyProvider provider;
    // Independent highest-level caches model two packages with unequal SMT
    // widths and cache sizes; CPU numbers carry no package/core relationship.
    provider.snapshot.caches[3].cpus = {2, 17, 2000};
    provider.snapshot.caches.push_back({101, 3, CE::Engine::WorkerCacheType::Unified, 8388608, 64, {901, 4097, 5001}});
    provider.inherited = {17, 901, 4097};
    const auto topology = CE::Engine::WorkerDetail::prepare_topology(provider);
    std::vector<CE::Engine::WorkerCacheDomain> roots;
    for (const auto& cache : topology.caches)
        if (cache.level == 3)
            roots.push_back(cache);
    ASSERT_EQ(roots.size(), 2u);
    EXPECT_EQ(roots[0].id, 99u);
    EXPECT_EQ(roots[0].cpus, (std::vector<unsigned int>{17}));
    EXPECT_EQ(roots[1].id, 101u);
    EXPECT_EQ(roots[1].cpus, (std::vector<unsigned int>{901, 4097}));
    EXPECT_NE(roots[0].capacity_bytes, roots[1].capacity_bytes);
    EXPECT_TRUE(CE::Engine::WorkerDetail::complete_core_topology(topology));
}

TEST(worker_configuration, names) {
    for (const auto* name : {"a", "simulation", "asset_preparation", "worker_42"})
        EXPECT_TRUE(CE::Engine::is_worker_group_name(name)) << name;
    for (const auto* name : {"", "1work", "_work", "Work", "a-b", "a.b", "a\n", "\xc3\xa9"})
        EXPECT_FALSE(CE::Engine::is_worker_group_name(name)) << name;
}

TEST(worker_configuration, validation) {
    CE::Engine::WorkerConfiguration configuration;
    configuration.pool.worker_count = 0;
    auto& options = configuration.groups["simulation"];
    EXPECT_NO_THROW(CE::Engine::validate_worker_configuration(configuration));
    options.weight = 0;
    EXPECT_THROW(CE::Engine::validate_worker_configuration(configuration), CE::Exceptions::invalid_args);
    options.weight = 1025;
    EXPECT_THROW(CE::Engine::validate_worker_configuration(configuration), CE::Exceptions::invalid_args);
    options.weight = 1024;
    options.priority = 8;
    EXPECT_THROW(CE::Engine::validate_worker_configuration(configuration), CE::Exceptions::invalid_args);
    options.priority = 7;
    options.cpu.cpus = {2, 2};
    EXPECT_THROW(CE::Engine::validate_worker_configuration(configuration), CE::Exceptions::invalid_args);
    options.cpu.cpus = {2, std::numeric_limits<unsigned int>::max()};
    options.cpu.locality = static_cast<CE::Engine::WorkerLocalityPolicy>(99);
    EXPECT_THROW(CE::Engine::validate_worker_configuration(configuration), CE::Exceptions::invalid_args);
    options.cpu.locality = CE::Engine::WorkerLocalityPolicy::Reuse;
    options.cpu.strength = static_cast<CE::Engine::WorkerPolicyStrength>(99);
    EXPECT_THROW(CE::Engine::validate_worker_configuration(configuration), CE::Exceptions::invalid_args);
    options.cpu.strength = CE::Engine::WorkerPolicyStrength::Required;
    options.cpu.numa_node = 0;
    EXPECT_THROW(CE::Engine::validate_worker_configuration(configuration), CE::Exceptions::invalid_args);
    options.cpu.numa_node.reset();
    EXPECT_NO_THROW(CE::Engine::validate_worker_configuration(configuration));
    configuration.groups["Bad"] = {};
    EXPECT_THROW(CE::Engine::validate_worker_configuration(configuration), CE::Exceptions::invalid_args);
}
