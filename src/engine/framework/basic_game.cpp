#include <engine/framework/basic_game.h>
#include <templates/delta.h>

namespace CE::GFramework {
    Game::~Game() {
        if (gf) {
            gf->deinit();
            e->deinit();
        }
    }

    void Game::run() {
        if (gf) {
            DeltaTime delta;
            gf->init();
            running = true;
            while(running) {
                const double dt = delta();
                e->pre_update();
                gf->update(dt);
                e->post_update();

                e->pre_draw();
                gf->draw(dt);
                e->post_draw();
            }
        }
    }

    void Game::stop() {
        running = false;
    }

}
