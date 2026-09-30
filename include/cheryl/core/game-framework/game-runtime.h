#pragma once

#include <core/controls/polling-backlog.h>

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace CE::Engine {
    class EngineContext;
}

namespace CE::GFramework {
    struct AbstractGame;

    enum class RunMode { Sequential, Concurrent };

    /** Coordinates platform input, game simulation, rendering, presentation, and teardown.
     * Each independently scheduled update receives accumulated State activity
     * and elapsed simulation time. Rendering consumes published render state, never the game's
     * live mutable simulation state. These boundaries
     * apply in either mode. The calling thread owns platform polling and graphics operations;
     * concurrent mode gives update and frame preparation to one simulation worker.
     * Sequential mode uses one recycled frame. Concurrent mode uses three slots
     * and renders the newest complete frame available at each handoff.
     */
    class GameRuntime final {
    public:
        GameRuntime(Engine::EngineContext& engine,
                    AbstractGame& game,
                    RunMode mode = RunMode::Sequential,
                    Input::PollingOptions polling = {});

        void run();
        void stop();

    private:
        void run_sequential();
        void run_concurrent();

        Engine::EngineContext& engine_;
        AbstractGame& game_;
        RunMode mode_;
        Input::PollingOptions polling_;
        std::atomic<bool> run_started_{false};
        std::atomic<bool> stop_requested_{false};
        std::mutex scheduler_mutex_;
        std::condition_variable scheduler_wake_;
    };
}
