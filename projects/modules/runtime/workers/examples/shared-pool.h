#pragma once

#include <core/engine/engine-context.h>
#include <core/engine/worker-configuration.h>

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace CE::Examples {
    struct WorkerComposition {
        // Member order keeps the application-owned root alive through context
        // shutdown. Context groups drain first; the application then joins its root.
        std::shared_ptr<CE::Workers::Pool> pool;
        std::unique_ptr<CE::Runtime::Context> context;
        std::map<std::string, CE::Workers::Group, std::less<>> groups;
    };

    using BackendFactory = std::function<std::unique_ptr<CE::Runtime::Context>(CE::Runtime::ExecutionOptions)>;

    [[nodiscard]] WorkerComposition compose_workers(CE::Workers::Configuration configuration, const BackendFactory& create_backend);
    [[nodiscard]] WorkerComposition compose_workers(const std::filesystem::path& source, const BackendFactory& create_backend);
}
