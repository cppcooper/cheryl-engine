#include <core/game-framework/game-runtime.h>

#include <internals/exceptions.h>

namespace CE::GFramework {
    GameRuntime::GameRuntime(Engine::EngineContext& engine, AbstractGame& game) : engine_(engine), game_(game) {}

    void GameRuntime::run() {
        // TODO: Pair each successful initialization with shutdown, including failure paths.
        // Initialize renderer and input, then game; collect all completed input polls;
        // assemble TickInput from polls since the last tick; update simulation with TickContext;
        // call prepare_render_frame() on the simulation thread, publish its value;
        // have the renderer consume it; present through the surface; deinitialize in reverse order.
        // Define the handoff policy and GPU resource teardown before scheduling threads.
        // Platform bootstrap must first create a compatible display/window/surface/renderer set.
        throw Exceptions::failed_operation(CE_HERE, "GameRuntime is a skeleton");
    }

    void GameRuntime::stop() { stop_requested_ = true; }
}
