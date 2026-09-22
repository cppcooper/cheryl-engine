#pragma once
#include <memory>
#include <utility>
#include "engines/abstract-engine.h"
#include "game-framework/abstract-game.h"

namespace CE::GFramework {
    template<typename T>
    using shptr = std::shared_ptr<T>;
    struct GameRuntime {
        explicit GameRuntime(shptr<Engine::iEngine> e, shptr<AbstractGame> gf) : e(std::move(e)), gf(std::move(gf)) {}
        ~GameRuntime();
        void run();
        void stop();
    protected:
        shptr<Engine::iEngine> e{};
        shptr<AbstractGame> gf{};
        bool running = false;
    };
}
