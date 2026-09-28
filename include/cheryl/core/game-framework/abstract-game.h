#pragma once

#include "tick-context.h"

namespace CE::GFramework {
    /** Game simulation hooks driven by GameRuntime.
     * update() receives input assembled for one simulation tick, regardless of whether
     * simulation runs on the platform thread or on a worker. The game is free to organize
     * its logic without a prescribed controller or state-machine architecture.
     * Drawing does not run on this interface. Simulation must publish complete render state
     * before a renderer can consume it; the representation and publication API remain to be defined.
     * The thread and service requirements of init()/deinit() also remain to be defined.
     */
    struct AbstractGame {
        virtual ~AbstractGame() = default;
        virtual void init() = 0;
        virtual void deinit() = 0;
        virtual void update(const TickContext& tick) = 0;
    };
}
