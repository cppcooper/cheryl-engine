#pragma once

#include <core/game-framework/game-runtime.h>

#include <functional>
#include <thread>

namespace CE::GFramework::RuntimeDetail {
    struct GameRuntimeAccess {
        // Configure before run. Return an owned joinable thread, or throw
        // without retaining the callable. Production directly uses std::thread.
        static void set_simulation_thread_factory(
            GameRuntime& runtime,
            std::function<std::thread(std::function<void()>)> factory
        );
    };
}
