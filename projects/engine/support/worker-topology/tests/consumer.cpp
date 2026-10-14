#include <cheryl/core/engine/worker-configuration.h>
#if CHERYL_CONSUMER_WORKER_JSON
#include <cheryl/core/engine/worker-topology/configuration.h>
#endif
#if CHERYL_CONSUMER_WORKER_HWLOC
#include <cheryl/core/engine/worker-topology/hwloc.h>
#endif

#include <exception>
#include <iostream>

#if defined(HWLOC_API_VERSION) || defined(NLOHMANN_JSON_VERSION_MAJOR)
#error Support headers must not expose their implementation SDKs.
#endif

int main() try {
    CE::Workers::Configuration configuration;
#if CHERYL_CONSUMER_WORKER_JSON
    configuration = CE::Workers::Json::parse_configuration(R"({"version":"1.0","workers":1,"groups":{"simulation":{"concurrency":1}}})");
#else
    configuration.groups.emplace("simulation", CE::Workers::GroupOptions{});
#endif
#if CHERYL_CONSUMER_WORKER_HWLOC
    configuration.pool.topology_provider = CE::Workers::Hwloc::make_topology_provider();
#endif
    CE::Workers::validate_configuration(configuration);
    CE::Workers::Pool pool(configuration.pool);
    auto group = pool.make_group(configuration.groups.at("simulation"));
    auto value = group.submit([] { return 11; });
    group.close();
    group.drain();
    return value.get() == 11 ? 0 : 1;
} catch (const std::exception& error) {
    std::cerr << "consumer-worker-topology failed: " << error.what() << '\n';
    return 1;
}
