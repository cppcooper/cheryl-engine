#include <cgl.h>
#include <core/game-runtime.h>
#include <core/engines/opengl-engine.h>
#include <templates/singleton.h>

class Game : public CE::GFramework::AbstractGame {
public:
    void init() override {

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
