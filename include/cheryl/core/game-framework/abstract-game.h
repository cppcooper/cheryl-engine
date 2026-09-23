#pragma once

namespace CE::GFramework {
    // TODO: Define the simulation/render handoff before update() and draw() can run concurrently.
    // Invoking both on the same game object would expose mutable simulation state to two threads;
    // prefer publishing an immutable/double-buffered render snapshot or command list at frame boundaries.
    struct AbstractGame {
        virtual ~AbstractGame() = default;
        virtual void init() = 0;
        virtual void deinit() = 0;
        virtual void update(double seconds) = 0;
        virtual void draw(double seconds) = 0;
    };
}
