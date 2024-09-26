#pragma once
#ifndef BASIC_GAME_H
#define BASIC_GAME_H
#include <memory>
#include <engine/engines/abstract-engine.h>
#include "abstract-game.h"

namespace CE::GFramework {
    template<typename T>
    using shptr = std::shared_ptr<T>;
    struct Game {
        explicit Game(shptr<Engine::iEngine> e, shptr<AbstractGame> gf) : e(e), gf(gf) {}
        ~Game();
        void run();
        void stop();
    protected:
        shptr<Engine::iEngine> e{};
        shptr<AbstractGame> gf{};
        bool running = false;
    };
}

#endif //BASIC_GAME_H
