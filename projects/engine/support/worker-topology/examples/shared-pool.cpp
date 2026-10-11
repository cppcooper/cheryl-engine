#include "shared-pool.h"
#include <core/engine/worker-topology/configuration.h>
#include <core/engine/worker-topology/hwloc.h>
#include <internals/exceptions.h>

#include <utility>

namespace CE::Examples {
    WorkerComposition compose_workers(Engine::WorkerConfiguration configuration, const BackendFactory& create_backend) {
        Engine::validate_worker_configuration(configuration);
        if (!create_backend)
            throw Exceptions::invalid_args(CE_HERE, "Worker composition requires a backend factory");
        if (!configuration.pool.topology_provider)
            configuration.pool.topology_provider = Engine::make_hwloc_worker_topology();

        WorkerComposition result;
        result.pool = std::make_shared<Engine::WorkerPool>(std::move(configuration.pool));
        Engine::ExecutionOptions execution;
        execution.shared_pool = result.pool;
        result.context = create_backend(std::move(execution));
        if (!result.context)
            throw Exceptions::failed_operation(CE_HERE, "Worker composition backend factory returned no context");
        for (const auto& [name, options] : configuration.groups)
            result.groups.emplace(name, result.context->make_worker_group(options));
        // Submit related work through result.groups.at("simulation"), for example.
        // Unrelated groups may use result.pool directly and retain their own lifetime.
        return result;
    }

    WorkerComposition compose_workers(const std::filesystem::path& source, const BackendFactory& create_backend) {
        return compose_workers(Engine::load_worker_configuration(source), create_backend);
    }
}
