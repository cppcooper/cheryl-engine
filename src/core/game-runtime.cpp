#include <thread>
#include <core/game-runtime.h>
#include <templates/delta.h>
#include <math/time.h>

namespace CE::GFramework {
    GameRuntime::~GameRuntime() {
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
            while(running) {
                const double dt = delta();
                gf->update(dt);

                e->pre_draw();
                gf->draw(dt);
                e->post_draw();
                std::this_thread::sleep_for(Milliseconds(15));
            }
        }
    }

    void GameRuntime::stop() {
        running = false;
    }

}
