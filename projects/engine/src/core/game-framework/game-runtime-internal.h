#pragma once

#include <cheryl/export.h>

#include <core/game-framework/game-runtime.h>
#include "runtime-pressure-internal.h"

#include <functional>
#include <thread>

namespace CE::Runtime::Detail {
    struct GameRuntimeAccess {
        [[nodiscard]] CE_API static std::shared_ptr<RuntimePressureSource> pressure_source(::CE::Runtime::GameRuntime& runtime);
        // Configure before run. Return an owned joinable thread, or throw
        // without retaining the callable. Production directly uses std::thread.
        CE_API static void
        set_simulation_thread_factory(::CE::Runtime::GameRuntime& runtime, std::function<std::thread(std::function<void()>)> factory);
    };
}
