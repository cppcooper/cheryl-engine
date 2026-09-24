#pragma once

namespace CE::GFramework {
    // TODO: Define the simulation/render handoff before update() and draw() can run concurrently.
    // Invoking both on the same game object would expose mutable simulation state to two threads;
    // prefer publishing an immutable/double-buffered render snapshot or command list at frame boundaries.
    // TODO: Give UI/HUD rendering a camera-independent pass and route pointer focus through UI
    // before gameplay bindings. The demo currently cancels camera pan manually to pin HUD text.
    /** Game hooks called in order by GameRuntime on the current runtime thread.
     * Input callbacks finish before update(); draw() follows engine frame preparation
     * and precedes buffer swap. Both hooks currently share the same mutable game object.
     */
    struct AbstractGame {
        virtual ~AbstractGame() = default;
        virtual void init() = 0;
        virtual void deinit() = 0;
        virtual void update(double seconds) = 0;
        virtual void draw(double seconds) = 0;
    };
}
