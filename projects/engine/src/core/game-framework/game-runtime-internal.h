#pragma once

#include <cheryl/export.h>

#include <core/game-framework/game-runtime.h>

#include <functional>
#include <thread>

namespace CE::Runtime::Detail {
    struct GameRuntimeAccess {
        // Configure before run. Return an owned joinable thread, or throw
        // without retaining the callable. Production directly uses std::thread.
        CE_API static void
        set_simulation_thread_factory(::CE::Runtime::GameRuntime& runtime, std::function<std::thread(std::function<void()>)> factory);
    };
}
