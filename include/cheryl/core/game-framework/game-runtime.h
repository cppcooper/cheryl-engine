#pragma once

namespace CE::Engine {
    class EngineContext;
}

namespace CE::GFramework {
    struct AbstractGame;

    /** Optional sequential game loop over engine services and game hooks.
     * A future concurrent scheduler must use the same complete input snapshot per tick;
     * simulation-to-render state will need its own publication boundary.
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
