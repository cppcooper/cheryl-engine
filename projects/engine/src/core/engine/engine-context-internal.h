#pragma once

#include <core/engine/engine-context.h>

#include <cstddef>
#include <functional>
#include <memory>

namespace CE::Runtime::Detail {
    // Per-context fault boundary for its lazy owned root. The normal path keeps
    // an empty factory and constructs CE::Workers::Pool directly; no global override.
    struct ContextAccess {
        static void
        set_owned_worker_factory(CE::Runtime::Context& context, std::function<std::unique_ptr<CE::Workers::Pool>(std::size_t)> factory);
    };
}
