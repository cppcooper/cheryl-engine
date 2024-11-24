#include <core/game-runtime.h>
#include <templates/delta.h>

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
            gf->init();
            running = true;
            while(running) {
                const double dt = delta();
                gf->update(dt);

                e->pre_draw();
                gf->draw(dt);
                e->post_draw();
            }
        }
    }

    void GameRuntime::stop() {
        running = false;
    }

}
