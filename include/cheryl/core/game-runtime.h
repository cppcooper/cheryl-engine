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
