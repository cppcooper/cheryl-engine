#include <cgl.h>
#include <core/controls/input-system.h>
#include <core/game-runtime.h>
#include <core/engines/opengl-engine.h>
#include <core/resources/asset-management/asset-loader.h>
#include <core/resources/asset-management/font-mgr.h>
#include <core/resources/asset-management/shader-mgr.h>
#include <core/resources/fileio/fonts-system.h>
#include <internals/exceptions.h>
#include <templates/asset-mgr.h>
#include <templates/singleton.h>

#include <ext/matrix_transform.hpp>

#include <filesystem>
#include <format>
#include <iostream>
#include <memory>
#include <string_view>
#include <utility>

class Game : public CE::GFramework::AbstractGame {
public:
    Game(std::shared_ptr<CE::Engine::glEngine> engine, std::filesystem::path asset_root, bool load_all_assets) :
        engine_(std::move(engine)), asset_root_(std::move(asset_root)), load_all_assets_(load_all_assets) {}

    void init() override {
        camera_ = std::make_shared<CE::Camera2D>();
        engine_->set_camera(camera_);

        const auto shader2d = asset_root_ / "shaders" / "shader2d";
        if (load_all_assets_) {
            CE::Assets::Loader::get(asset_root_).load_assets();
        }
        else {
            const auto font_path = CE::Resources::select_default_system_font(CE::Resources::find_system_fonts());
            if (!font_path)
                throw CE::Exceptions::runtime_exception(CE_HERE, "No supported system font was found");
            CE::Assets::FontMgr::get().load_assets({*font_path});
            CE::Assets::ShaderMgr::get().load_program(shader2d,
                                                      {shader2d.string() + ".vert", shader2d.string() + ".frag"});
        }
        font_ = CE::Assets::FontMgr::get().default_font();
        font_shader_ = CE::Assets::ShaderMgr::get().get_asset(shader2d);
        if (!font_)
            throw CE::Exceptions::runtime_exception(CE_HERE, "No supported system font was found");
        if (!font_shader_)
            throw CE::Exceptions::runtime_exception(CE_HERE, "The shader2d program was not loaded");

        auto& input = CE::Input::InputSystem::get();
        auto& bindings = input.bindings();
        const auto keyboard = input.keyboard_id();
        const auto bind_direction = [&](const gainput::DeviceButtonId key, bool* held) {
            bindings.bind_button({keyboard, key}, [held](bool, bool pressed) { *held = pressed; });
        };
        bind_direction(gainput::KeyW, &up_);
        bind_direction(gainput::KeyA, &left_);
        bind_direction(gainput::KeyS, &down_);
        bind_direction(gainput::KeyD, &right_);
        bindings.bind_button({keyboard, gainput::KeyR}, [this](bool, bool pressed) {
            if (pressed) {
                pan_ = {0.0f, 0.0f};
                camera_->set_view_matrix(glm::mat4(1.0f));
            }
        });

        const auto mouse = input.mouse_id();
        bindings.bind_axis({mouse, gainput::MouseAxisX}, [this](float, float current) { mouse_x_ = current; });
        bindings.bind_axis({mouse, gainput::MouseAxisY}, [this](float, float current) { mouse_y_ = current; });
        bindings.bind_button({mouse, gainput::MouseButtonLeft}, [this](bool, bool pressed) {
            if (pressed)
                ++clicks_;
        });
        bindings.bind_button({mouse, gainput::MouseButtonWheelUp}, [this](bool, bool pressed) {
            if (pressed)
                ++wheel_;
        });
        bindings.bind_button({mouse, gainput::MouseButtonWheelDown}, [this](bool, bool pressed) {
            if (pressed)
                --wheel_;
        });
        bindings.bind_button({input.gamepad_id(), gainput::PadButtonA}, [this](bool, bool pressed) {
            if (pressed)
                ++gamepad_presses_;
        });
    }

    void deinit() override { CE::Input::InputSystem::get().bindings().clear(); }

    void update(const double seconds) override {
        const glm::vec2 movement{static_cast<float>(right_) - static_cast<float>(left_),
                                 static_cast<float>(up_) - static_cast<float>(down_)};
        if (glm::length(movement) > 0.0f) {
            pan_ += glm::normalize(movement) * static_cast<float>(seconds * 240.0);
            camera_->set_view_matrix(glm::translate(glm::mat4(1.0f), glm::vec3(-pan_, 0.0f)));
        }
    }

    void draw(double) override {
        const auto size = camera_->framebuffer_size();
        CE::Assets::FontDrawInfo text;
        text.material = font_shader_;
        text.position = {static_cast<float>(size.width) * 0.5f - 120.0f, static_cast<float>(size.height) * 0.5f, 0.0f};
        font_->print("Camera target", &text);

        // Compensate for the view translation so these controls stay fixed on screen.
        text.position = {pan_.x + 24.0f, pan_.y + static_cast<float>(size.height) - 56.0f, 0.0f};
        font_->print(std::format("Cheryl Engine demo\nWASD: pan camera  R: reset\n"
                                 "Mouse: {:.2f}, {:.2f}  Clicks: {}  Wheel: {}\nGamepad A: {} presses",
                                 mouse_x_, mouse_y_, clicks_, wheel_, gamepad_presses_),
                     &text);
        GLenum err;
        while ((err = glGetError()) != GL_NO_ERROR) {
            std::cerr << "OpenGL error: " << err << "\n";
        }
    }

private:
    std::shared_ptr<CE::Engine::glEngine> engine_;
    std::shared_ptr<CE::Camera2D> camera_;
    std::filesystem::path asset_root_;
    bool load_all_assets_;
    std::shared_ptr<CE::Assets::Font> font_;
    std::shared_ptr<CE::Assets::GLSLProgram> font_shader_;
    glm::vec2 pan_{0.0f, 0.0f};
    float mouse_x_ = 0.0f;
    float mouse_y_ = 0.0f;
    bool up_ = false;
    bool left_ = false;
    bool down_ = false;
    bool right_ = false;
    int clicks_ = 0;
    int wheel_ = 0;
    int gamepad_presses_ = 0;
};

using CE::Engine::glEngine;
using CE::GFramework::GameRuntime;

int main(const int argc, char** argv) {
    std::filesystem::path asset_root = std::filesystem::path(CHERYL_SOURCE_DIR) / "assets";
    bool load_all_assets = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--full-assets")
            load_all_assets = true;
        else
            asset_root = argv[i];
    }
    auto engine = std::make_shared<glEngine>();
    GameRuntime game_runtime(engine, std::make_shared<Game>(engine, asset_root, load_all_assets));
    game_runtime.run();
}
