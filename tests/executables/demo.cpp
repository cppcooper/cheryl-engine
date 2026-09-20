#include <cgl.h>
#include <core/game-runtime.h>
#include <core/engines/opengl-engine.h>
#include <core/resources/asset-management/asset-loader.h>
#include <templates/asset-mgr.h>
#include <templates/singleton.h>

#include <filesystem>
#include <utility>

class Game : public CE::GFramework::AbstractGame {
public:
    explicit Game(std::filesystem::path asset_root)
        : asset_root_(std::move(asset_root)) {}

    void init() override {
        CE::Assets::Loader::get(asset_root_).load_assets();
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

private:
    std::filesystem::path asset_root_;
};

using CE::Engine::glEngine;
using CE::GFramework::GameRuntime;

int main(const int argc, char** argv) {
    const std::filesystem::path asset_root = argc > 1 ? argv[1] : "assets";
    GameRuntime game_runtime(
        std::make_shared<glEngine>(), std::make_shared<Game>(asset_root));
    game_runtime.run();
}
