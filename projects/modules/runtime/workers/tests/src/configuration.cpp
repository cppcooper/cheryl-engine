#include <core/engine/worker-topology/configuration.h>
#include <internals/exceptions.h>
#include <testing/worker-topology.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
    constexpr auto document = R"({
        "version": "1.0", "workers": "auto",
        "groups": {
            "simulation": {"concurrency": 1, "weight": 3, "priority": 2, "locality": "reuse",
                "cpu": {"cpus": [2000, 17], "strength": "required", "cache_domain": 88, "prefer_same_worker": false}},
            "asset_preparation": {"concurrency": "auto", "locality": "spread"}
        }
    })";

    void same_policy(const CE::Workers::GroupPolicy& actual, const CE::Workers::GroupPolicy& expected) {
        EXPECT_EQ(actual.requested.max_concurrency, expected.requested.max_concurrency);
        EXPECT_EQ(actual.requested.weight, expected.requested.weight);
        EXPECT_EQ(actual.requested.priority, expected.requested.priority);
        EXPECT_EQ(actual.requested.cpu.cpus, expected.requested.cpu.cpus);
        EXPECT_EQ(actual.requested.cpu.strength, expected.requested.cpu.strength);
        EXPECT_EQ(actual.requested.cpu.locality, expected.requested.cpu.locality);
        EXPECT_EQ(actual.requested.cpu.prefer_same_worker, expected.requested.cpu.prefer_same_worker);
        EXPECT_EQ(actual.effective_cpus, expected.effective_cpus);
        EXPECT_EQ(actual.effective_locality, expected.effective_locality);
        EXPECT_EQ(actual.effective_cache_domain, expected.effective_cache_domain);
        EXPECT_EQ(actual.effective_max_concurrency, expected.effective_max_concurrency);
        EXPECT_EQ(actual.core_topology, expected.core_topology);
        EXPECT_EQ(actual.preferred_fallback, expected.preferred_fallback);
    }
}

TEST(worker_json, cpp_equivalence) {
    auto loaded = CE::Workers::Json::parse_configuration(document);
    CE::Workers::Configuration typed;
    typed.pool.worker_count = 0;
    auto& simulation = typed.groups["simulation"];
    simulation.max_concurrency = 1;
    simulation.weight = 3;
    simulation.priority = 2;
    simulation.cpu.cpus = {2000, 17};
    simulation.cpu.strength = CE::Workers::PolicyStrength::Required;
    simulation.cpu.locality = CE::Workers::LocalityPolicy::Reuse;
    simulation.cpu.shared_cache_domain = 88;
    simulation.cpu.prefer_same_worker = false;
    typed.groups["asset_preparation"].cpu.locality = CE::Workers::LocalityPolicy::Spread;
    auto provider = std::make_shared<CE::Testing::WorkerTopologyProvider>();
    loaded.pool.topology_provider = provider;
    typed.pool.topology_provider = provider;
    CE::Workers::validate_configuration(loaded);
    CE::Workers::validate_configuration(typed);
    EXPECT_EQ(provider->discoveries, 0u);
    EXPECT_TRUE(provider->bindings().empty());
    CE::Workers::Pool from_json(loaded.pool);
    CE::Workers::Pool from_cpp(typed.pool);
    EXPECT_EQ(from_json.worker_count(), from_cpp.worker_count());
    for (const auto& [name, options] : loaded.groups)
        same_policy(from_json.make_group(options).policy(), from_cpp.make_group(typed.groups.at(name)).policy());
}

TEST(worker_json, sources) {
    std::istringstream input(document);
    const auto streamed = CE::Workers::Json::load_configuration(input, "workers.json");
    EXPECT_EQ(streamed.groups.size(), 2u);
    EXPECT_EQ(streamed.pool.worker_count, 0u);
    EXPECT_FALSE(streamed.pool.topology_provider);
    const auto example = std::filesystem::path{CHERYL_SOURCE_DIR} / "projects/modules/runtime/workers/examples/workers.json";
    const auto loaded = CE::Workers::Json::load_configuration(example);
    EXPECT_EQ(loaded.groups.at("simulation").cpu.locality, CE::Workers::LocalityPolicy::Reuse);
    EXPECT_EQ(loaded.groups.at("asset_preparation").cpu.locality, CE::Workers::LocalityPolicy::Spread);
    EXPECT_THROW(static_cast<void>(CE::Workers::Json::load_configuration(example / "missing")), CE::Exceptions::invalid_args);
    input.setstate(std::ios::badbit);
    EXPECT_THROW(static_cast<void>(CE::Workers::Json::load_configuration(input, "broken.json")), CE::Exceptions::invalid_args);
}

TEST(worker_json, defaults) {
    const auto configuration = CE::Workers::Json::parse_configuration(R"({"version":"1.0","groups":{"a":{}}})");
    EXPECT_EQ(configuration.pool.worker_count, 1u);
    const auto& options = configuration.groups.at("a");
    EXPECT_EQ(options.max_concurrency, 0u);
    EXPECT_EQ(options.weight, 1u);
    EXPECT_EQ(options.priority, 0u);
    EXPECT_FALSE(options.cpu.locality);
    EXPECT_TRUE(options.cpu.cpus.empty());
    EXPECT_EQ(options.cpu.strength, CE::Workers::PolicyStrength::Preferred);
}

TEST(worker_json, invalid_fields) {
    const std::vector<std::pair<std::string_view, std::string_view>> invalid{{R"({})", "$.version"}, {R"({"version":"2.0"})", "$.version"},
        {R"({"version":1})", "$.version"}, {R"({"version":"1.0","extra":true})", "$.extra"},
        {R"({"version":"1.0","workers":0})", "$.workers"}, {R"({"version":"1.0","workers":-1})", "$.workers"},
        {R"({"version":"1.0","workers":1.0})", "$.workers"}, {R"({"version":"1.0","workers":18446744073709551616})", "$.workers"},
        {R"({"version":"1.0","workers":true})", "$.workers"}, {R"({"version":"1.0","workers":"automatic"})", "$.workers"},
        {R"({"version":"1.0","groups":[]})", "$.groups"}, {R"({"version":"1.0","groups":{"Bad":{}}})", "$.groups.Bad"},
        {R"({"version":"1.0","groups":{"a\n":{}}})", "$.groups.a"},
        {R"({"version":"1.0","groups":{"a":{"concurrency":0}}})", "$.groups.a.concurrency"},
        {R"({"version":"1.0","groups":{"a":{"weight":1025}}})", "$.groups.a.weight"},
        {R"({"version":"1.0","groups":{"a":{"weight":0}}})", "$.groups.a.weight"},
        {R"({"version":"1.0","groups":{"a":{"priority":8}}})", "$.groups.a.priority"},
        {R"({"version":"1.0","groups":{"a":{"locality":"numa"}}})", "$.groups.a.locality"},
        {R"({"version":"1.0","groups":{"a":{"unknown":1}}})", "$.groups.a.unknown"},
        {R"({"version":"1.0","groups":{"a":{"cpu":{"cpus":"2"}}}})", "$.groups.a.cpu.cpus"},
        {R"({"version":"1.0","groups":{"a":{"cpu":{"cpus":[-1]}}}})", "$.groups.a.cpu.cpus[0]"},
        {R"({"version":"1.0","groups":{"a":{"cpu":{"cpus":[4294967296]}}}})", "$.groups.a.cpu.cpus[0]"},
        {R"({"version":"1.0","groups":{"a":{"cpu":{"cpus":[2,2]}}}})", "$.groups.a"},
        {R"({"version":"1.0","groups":{"a":{"cpu":{"strength":"exclusive"}}}})", "$.groups.a.cpu.strength"},
        {R"({"version":"1.0","groups":{"a":{"cpu":{"prefer_same_worker":1}}}})", "$.groups.a.cpu.prefer_same_worker"},
        {R"({"version":"1.0","groups":{"a":{"cpu":{"numa_node":0}}}})", "$.groups.a.cpu.numa_node"},
        {R"({"version":"1.0","version":"1.0"})", "duplicate"},
        {R"({"version":"1.0","groups":{"a":{"cpu":{"cpus":[],"cpus":[]}}}})", "duplicate"}, {"{", "parse"},
        {R"({"version":"1.0"} {})", "parse"}};
    for (const auto& [value, field] : invalid) {
        SCOPED_TRACE(value);
        try {
            static_cast<void>(CE::Workers::Json::parse_configuration(value, "invalid-workers.json"));
            ADD_FAILURE() << "Invalid configuration was accepted";
        } catch (const CE::Exceptions::invalid_args& error) {
            EXPECT_NE(std::string{error.what()}.find("invalid-workers.json"), std::string::npos);
            EXPECT_NE(std::string{error.what()}.find(field), std::string::npos);
        }
    }
}
