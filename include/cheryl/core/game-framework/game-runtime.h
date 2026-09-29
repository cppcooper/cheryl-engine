#pragma once

#include <atomic>

namespace CE::Engine {
    class EngineContext;
}

namespace CE::GFramework {
    struct AbstractGame;

    enum class RunMode { Sequential, Concurrent };

    /** Coordinates platform input, game simulation, rendering, presentation, and teardown.
     * Each simulation tick receives a TickInput assembled from completed polls. Rendering consumes only
     * published render state, never the game's live mutable simulation state. These boundaries
     * apply in either mode. The calling thread owns platform polling and graphics operations;
     * concurrent mode gives update and frame preparation to one simulation worker.
     * The scheduler and ownership handoff for reusable frame slots remain to be implemented.
     */
    class GameRuntime final {
    public:
        GameRuntime(Engine::EngineContext& engine, AbstractGame& game, RunMode mode = RunMode::Sequential);

        void run();
        void stop();

    private:
        void run_sequential();
        void run_concurrent();

        Engine::EngineContext& engine_;
        AbstractGame& game_;
        RunMode mode_;
        std::atomic<bool> stop_requested_{false};
    };
}
