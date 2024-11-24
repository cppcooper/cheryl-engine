#pragma once
#ifndef GAMERUNTIME_H
#define GAMERUNTIME_H
#include <memory>
#include "engines/abstract-engine.h"
#include "game-framework/abstract-game.h"

namespace CE::GFramework {
    template<typename T>
    using shptr = std::shared_ptr<T>;
    struct GameRuntime {
        explicit GameRuntime(shptr<Engine::iEngine> e, shptr<AbstractGame> gf) : e(e), gf(gf) {}
        ~GameRuntime();
        void run();
        void stop();
    protected:
        shptr<Engine::iEngine> e{};
        shptr<AbstractGame> gf{};
        bool running = false;
    };
}

#endif //GAMERUNTIME_H
