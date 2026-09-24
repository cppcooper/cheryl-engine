#pragma once
#include <memory>
#include <utility>
#include "engines/abstract-engine.h"
#include "game-framework/abstract-game.h"

namespace CE::GFramework {
    template<typename T>
    using shptr = std::shared_ptr<T>;

    // TODO: Make the execution model explicit before adding/restoring multithreading. Platform/input,
    // simulation, render, and general worker responsibilities should exchange queued events or immutable
    // snapshots at defined boundaries instead of sharing live mutable state between long-lived threads.
    // TODO: Add engine-owned services at the frame boundaries rather than inside rendering or GLFW input:
    // - Initialize/deinitialize an audio device alongside other engine services; submit sound commands from
    //   the simulation, with an explicit audio-thread/teardown contract and listener state from the game camera.
    // - Poll network transport independently of GLFW, queue decoded messages for a simulation tick, and
    //   serialize authoritative state/snapshots after that tick; never mutate live game state from I/O callbacks.
    //   A versioned simulation snapshot could also support saves and deterministic replay.
    // - Establish a world/entity update contract before fixed-step physics and collision, AI decisions, or
    //   pathfinding. Navigation jobs can read immutable world snapshots and return results for a later tick;
    //   rendering and audio should consume published results rather than half-updated world state.
    /** Owns the order of platform events, game update, and frame presentation.
     * The engine supplies platform/renderer adapters; the game supplies simulation and draw hooks.
     */
    struct GameRuntime {
        explicit GameRuntime(shptr<Engine::iEngine> e, shptr<AbstractGame> gf) : e(std::move(e)), gf(std::move(gf)) {}
        ~GameRuntime();
        void run();
        void stop();
    protected:
        shptr<Engine::iEngine> e{};
        shptr<AbstractGame> gf{};
        // TODO: If stop() becomes a cross-thread operation, replace this plain flag with a synchronized
        // cancellation mechanism such as std::stop_token or an atomic with a documented memory contract.
        bool running = false;
    };
}
