#pragma once

namespace CE::Engine {
    class EngineContext;
}

namespace CE::GFramework {
    struct AbstractGame;

    /** Coordinates platform input, game simulation, rendering, presentation, and teardown.
     * Each simulation tick receives a TickInput assembled from completed polls. Rendering consumes only
     * published render state, never the game's live mutable simulation state. These boundaries
     * apply whether the runtime executes sequentially or schedules simulation separately.
     * The scheduler and frame handoff policy remain to be implemented.
     */
    class GameRuntime final {
    public:
        GameRuntime(Engine::EngineContext& engine, AbstractGame& game);

        void run();
        void stop();

    private:
        Engine::EngineContext& engine_;
        AbstractGame& game_;
        bool stop_requested_ = false; // Same-thread only until a cancellation contract exists.
    };
}
