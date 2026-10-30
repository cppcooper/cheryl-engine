#pragma once

#include <cheryl/export.h>

#include <core/game-framework/game-runtime.h>
#include <core/subsystems/event-bus.h>
#include "runtime-pressure-internal.h"

#include <functional>
#include <thread>

namespace CE::Runtime::Detail {
    inline const ::CE::Events::EventChannel<RuntimePressureChanged> runtime_pressure_changed_event{"runtime-pressure-changed"};

    struct RuntimePressureWatch {
        ::CE::Events::EventBus::Registration registration;
        RuntimePressureSnapshot seed; // Current state; queued consumers reject older/equal revisions.
    };

    struct GameRuntimeAccess {
        [[nodiscard]] CE_API static std::shared_ptr<RuntimePressureState> pressure_state(::CE::Runtime::GameRuntime& runtime);
        // Synchronized pre-run setup or the active platform owner. Routing always
        // uses the runtime's Context; attachment never chooses a separate bus.
        [[nodiscard]] CE_API static RuntimePressureWatch
        watch_pressure(::CE::Runtime::GameRuntime& runtime, ::CE::Events::EventChannel<RuntimePressureChanged>::Callback callback);
        CE_API static void publish_pressure(::CE::Runtime::GameRuntime& runtime);
        // Configure before run. Return an owned joinable thread, or throw
        // without retaining the callable. Production directly uses std::thread.
        CE_API static void
        set_simulation_thread_factory(::CE::Runtime::GameRuntime& runtime, std::function<std::thread(std::function<void()>)> factory);
    };
}
