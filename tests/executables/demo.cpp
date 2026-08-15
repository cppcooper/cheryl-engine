#include <cgl.h>
#include <core/game-runtime.h>
#include <core/engines/opengl-engine.h>
#include <core/resources/asset-management/asset-loader.h>
#include <templates/asset-mgr.h>
#include <templates/singleton.h>
#include <math/round.h>

class Game : public CE::GFramework::AbstractGame {
public:
    void init() override {
        // todo: get root directory
        CE::Assets::Loader::get("/home/jcooper/Documents/projects/cheryl-engine/assets");
    }
    void deinit() override {

    }
    void update(double seconds) override {

    }
    void draw(double seconds) override {
        GLenum err;
        while ((err = glGetError()) != GL_NO_ERROR) {
            std::cerr << "OpenGL error: " << err << "\n";
        }
    }
};

using CE::Engine::glEngine;
using CE::GFramework::GameRuntime;

int main() {
    GameRuntime game_runtime(std::make_shared<glEngine>(),std::make_shared<Game>());
    game_runtime.run();
}
