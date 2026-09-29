#include <core/game-framework/game-runtime.h>

#include <internals/exceptions.h>

namespace CE::GFramework {
    GameRuntime::GameRuntime(Engine::EngineContext& engine, AbstractGame& game, const RunMode mode) :
        engine_(engine), game_(game), mode_(mode) {}

    void GameRuntime::run() {
        switch (mode_) {
        case RunMode::Sequential:
            return run_sequential();
        case RunMode::Concurrent:
            return run_concurrent();
        }
        throw Exceptions::invalid_args(CE_HERE, "Unknown game runtime mode");
    }

    void GameRuntime::run_sequential() {
        // TODO: Pair each successful initialization with shutdown, including failure paths.
        // Initialize renderer and input, then game; collect all completed input polls;
        // assemble TickInput from polls since the last tick; update simulation with TickContext;
        // prepare a recycled frame slot, render and present, then recycle commands.
        // Platform bootstrap must first create a compatible display/window/surface/renderer set.
        throw Exceptions::failed_operation(CE_HERE, "Sequential game runtime is a skeleton");
    }

    void GameRuntime::run_concurrent() {
        // TODO: After platform-thread initialization, start one worker for update and frame preparation.
        // Keep input polling, rendering, presentation, and frame recycling on the calling thread;
        // join the worker and release game/GPU resources before leaving the graphics context.
        // A stop request must also wake any worker waiting for input or a free frame slot.
        throw Exceptions::failed_operation(CE_HERE, "Concurrent game runtime is a skeleton");
    }

    void GameRuntime::stop() { stop_requested_.store(true, std::memory_order_release); }
}
