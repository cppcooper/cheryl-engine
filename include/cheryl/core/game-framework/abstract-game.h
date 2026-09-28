#pragma once

#include <core/controls/action-snapshot.h>

namespace CE::GFramework {
    /** Game simulation hooks driven by GameRuntime.
     * update_with_input() receives one completed input poll. Games may instead retain the
     * original update() hook; no controller or state-machine architecture is required.
     * Drawing does not run on this interface. Simulation must publish complete render state
     * before a renderer can consume it; the representation and publication API remain to be defined.
     * The thread and service requirements of init()/deinit() also remain to be defined.
     */
    struct AbstractGame {
        virtual ~AbstractGame() = default;
        virtual void init() = 0;
        virtual void deinit() = 0;
        virtual void update(double) {}
        virtual void update_with_input(double seconds, const Input::ActionSnapshot&) { update(seconds); }
    };
}
