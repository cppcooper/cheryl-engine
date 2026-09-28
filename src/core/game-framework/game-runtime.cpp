#include <core/game-framework/game-runtime.h>

#include <internals/exceptions.h>

namespace CE::GFramework {
    GameRuntime::GameRuntime(Engine::EngineContext& engine, AbstractGame& game) : engine_(engine), game_(game) {}

    void GameRuntime::run() {
        // TODO: Pair each successful initialization with shutdown, including failure paths.
        // Initialize renderer and input, then game; poll input and pin one ActionSnapshot;
        // update simulation; publish complete render state; have the renderer consume that
        // published state; present through the surface; deinitialize in reverse order.
        // Define the frame representation and publication policy before scheduling threads.
        // Platform bootstrap must first create a compatible display/window/surface/renderer set.
        throw Exceptions::failed_operation(CE_HERE, "GameRuntime is a skeleton");
    }

    void GameRuntime::stop() { stop_requested_ = true; }
}
