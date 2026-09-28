#pragma once

#include <core/controls/action-snapshot.h>

namespace CE::GFramework {
    // TODO: Define the simulation/render handoff before update() and draw() can run concurrently.
    // Invoking both on the same game object would expose mutable simulation state to two threads;
    // prefer publishing an immutable/double-buffered render snapshot or command list at frame boundaries.
    // TODO: Give UI/HUD rendering a camera-independent pass and route pointer focus through UI
    // before gameplay bindings. The demo currently cancels camera pan manually to pin HUD text.
    /** Optional sequential game hooks driven by GameRuntime.
     * update_with_input() receives one completed input poll. Games may instead retain the
     * original update() hook; no controller or state-machine architecture is required.
     * Draw follows frame preparation and precedes surface presentation.
     */
    struct AbstractGame {
        virtual ~AbstractGame() = default;
        virtual void init() = 0;
        virtual void deinit() = 0;
        virtual void update(double) {}
        virtual void update_with_input(double seconds, const Input::ActionSnapshot&) { update(seconds); }
        virtual void draw(double seconds) = 0;
    };
}
