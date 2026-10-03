#pragma once

#include <core/engine/engine-context.h>

#include <cstddef>
#include <functional>
#include <memory>

namespace CE::Engine::ContextDetail {
    // Per-context fault boundary for its lazy owned root. The normal path keeps
    // an empty factory and constructs WorkerPool directly; no global override.
    struct EngineContextAccess {
        static void set_owned_worker_factory(EngineContext& context, std::function<std::unique_ptr<WorkerPool>(std::size_t)> factory);
    };
}
