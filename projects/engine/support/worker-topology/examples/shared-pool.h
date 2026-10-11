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
        std::shared_ptr<Engine::WorkerPool> pool;
        std::unique_ptr<Engine::EngineContext> context;
        std::map<std::string, Engine::WorkerGroup, std::less<>> groups;
    };

    using BackendFactory = std::function<std::unique_ptr<Engine::EngineContext>(Engine::ExecutionOptions)>;

    [[nodiscard]] WorkerComposition compose_workers(Engine::WorkerConfiguration configuration, const BackendFactory& create_backend);
    [[nodiscard]] WorkerComposition compose_workers(const std::filesystem::path& source, const BackendFactory& create_backend);
}
