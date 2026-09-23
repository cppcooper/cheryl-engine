#include <thread>
#include <core/game-runtime.h>
#include <templates/delta.h>
#include <math/time.h>

namespace CE::GFramework {
    GameRuntime::~GameRuntime() {
        // TODO: Track successful engine/game initialization explicitly so deinit is paired only
        // with completed init calls, including when run() exits through an initialization exception.
        // A threaded runtime also needs a stop/join barrier here so no simulation, render, input, or
        // worker job can still touch engine/game resources while deinitialization tears them down.
        if (gf) {
            gf->deinit();
            e->deinit();
        }
    }

    void GameRuntime::run() {
        if (gf) {
            DeltaTime delta;
            e->init();
            gf->init();
            running = true;
            // TODO: Keep this sequential ordering as one valid scheduler mode. If update and render are
            // decoupled, drive simulation from its own timestep and hand immutable frame state to rendering
            // rather than simply executing gf->update() and gf->draw() concurrently on the same object.
            while (running) {
                e->poll_input();
                if (e->should_close())
                    break;
                const double dt = delta();
                gf->update(dt);

                e->pre_draw();
                gf->draw(dt);
                e->post_draw();
                // TODO: Replace the fixed sleep with an explicit frame-pacing policy. It compounds
                // swap-interval blocking and hard-codes cadence into the runtime instead of timing
                // configuration or a target-frame scheduler.
                std::this_thread::sleep_for(Milliseconds(15));
            }
        }
    }

    void GameRuntime::stop() {
        running = false;
    }

}
