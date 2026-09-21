#include <cgl.h>
#include <core/game-runtime.h>
#include <core/engines/opengl-engine.h>
#include <core/resources/asset-management/asset-loader.h>
#include <core/resources/asset-management/font-mgr.h>
#include <core/resources/asset-management/shader-mgr.h>
#include <internals/exceptions.h>
#include <templates/asset-mgr.h>
#include <templates/singleton.h>

#include <filesystem>
#include <iostream>
#include <memory>
#include <utility>

class Game : public CE::GFramework::AbstractGame {
public:
    explicit Game(std::filesystem::path asset_root) : asset_root_(std::move(asset_root)) {}

    void init() override {
        CE::Assets::Loader::get(asset_root_).load_assets();
        font_ = CE::Assets::FontMgr::get().default_font();
        font_shader_ = CE::Assets::ShaderMgr::get().get_asset(asset_root_ / "shaders" / "shader2d");
        if (!font_)
            throw CE::Exceptions::runtime_exception(CE_HERE, "No supported system font was found");
        if (!font_shader_)
            throw CE::Exceptions::runtime_exception(CE_HERE, "The shader2d program was not loaded");
    }
    void deinit() override {}
    void update(double seconds) override {}
    void draw(double seconds) override {
        CE::Assets::FontDrawInfo text;
        text.material = font_shader_;
        text.position = {64.0f, 96.0f, 0.0f};
        font_->print("Cheryl Engine: system-font rendering", &text);
        GLenum err;
        while ((err = glGetError()) != GL_NO_ERROR) {
            std::cerr << "OpenGL error: " << err << "\n";
        }
    }

private:
    std::filesystem::path asset_root_;
    std::shared_ptr<CE::Assets::Font> font_;
    std::shared_ptr<CE::Assets::GLSLProgram> font_shader_;
};

using CE::Engine::glEngine;
using CE::GFramework::GameRuntime;

int main(const int argc, char** argv) {
    const std::filesystem::path asset_root = argc > 1 ? argv[1] : "assets";
    GameRuntime game_runtime(std::make_shared<glEngine>(), std::make_shared<Game>(asset_root));
    game_runtime.run();
}
