#include "game.h"

#include <cheryl/core/controls/button-codes.h>
#include <cheryl/core/controls/input-interface.h>
#include <cheryl/core/display/window-interface.h>
#include <cheryl/core/resources/asset-management/asset-loader.h>
#include <cheryl/core/resources/asset-management/shader-asset-mgr.h>
#include <cheryl/core/resources/asset-management/sprite-mgr.h>

#include <chrono>
#include <limits>
#include <stdexcept>
#include <utility>

namespace Tutorial {
    Game::Game(CE::Runtime::Context& engine, std::filesystem::path assets)
    : engine_(engine), asset_root_(std::move(assets)) {}

    void Game::init() {
        world_camera_.set_framebuffer_size(engine_.window().framebuffer_size());
        hud_camera_.set_framebuffer_size(engine_.window().framebuffer_size());

        // [bindings]
        auto& input = engine_.input();
        auto& bindings = input.bindings();
        const auto keyboard = input.keyboard_id();
        bindings_[0] = bindings.bind_button({keyboard, CE::Input::KeyboardKey::Escape}, quit_);
        bindings_[1] = bindings.bind_button({keyboard, CE::Input::KeyboardKey::Left}, left_);
        bindings_[2] = bindings.bind_button({keyboard, CE::Input::KeyboardKey::Right}, right_);
        bindings_[3] = bindings.bind_button({keyboard, CE::Input::KeyboardKey::Space}, score_);
        bindings_[4] = bindings.bind_button({keyboard, CE::Input::KeyboardKey::A}, pan_left_);
        bindings_[5] = bindings.bind_button({keyboard, CE::Input::KeyboardKey::D}, pan_right_);
        // [/bindings]

        // [assets]
        CE::Assets::Loading::Loader loader(asset_root_);
        loader.load_assets(engine_.resources());
        sprite_ = CE::Assets::Sprites::Manager::get().get_asset("first-game", "actor");
        images_ = CE::Assets::Shaders::Manager::get().find_material("first-game:images");
        text_ = CE::Assets::Shaders::Manager::get().find_material("first-game:text");
        if (!sprite_ || !images_ || !text_)
            throw std::runtime_error("Tutorial assets require actor, images and text resources");
        player_.emplace(sprite_, images_, glm::vec2(160, 100), left_, right_);
        enemy_.emplace(sprite_, images_, 80, 500, 220);
        animation_.emplace(sprite_->animation("blink"));
        // [/assets]

        CE::Text::FontSelection selection;
        selection.preferred = {"DejaVu Sans"};
        selection.asset_directories = {asset_root_};
        selection.automatic_system_fonts = false;
        hud_.emplace(engine_, CE::Text::FontCollection::load(selection), text_);
        controls_.emplace(engine_);
    }

    // [update]
    void Game::update(const CE::Game::TickContext& tick) {
        world_camera_.set_framebuffer_size(tick.framebuffer_size);
        hud_camera_.set_framebuffer_size(tick.framebuffer_size);
        if (tick.input.button(quit_).pressed() && controls_->permits_quit()) {
            tick.request_stop();
            return;
        }
        player_->update(tick);
        enemy_->update(tick.delta_seconds);
        animation_->advance(std::chrono::duration<double>(tick.delta_seconds));

        const float pan = (tick.input.button(pan_right_).held() ? 1.0f : 0.0f) - (tick.input.button(pan_left_).held() ? 1.0f : 0.0f);
        view_origin_.x += pan * 100.0f * static_cast<float>(tick.delta_seconds);
        world_camera_.set_view_matrix(glm::translate(glm::mat4(1.0f), glm::vec3(-view_origin_, 0.0f)));

        const bool applied = controls_->update(tick);
        if ((tick.input.button(score_).pressed() || applied) && points_ < std::numeric_limits<int>::max())
            hud_->set_score(++points_);
        hud_->update();
    }
    // [/update]

    // [render]
    void Game::prepare_render_frame(CE::Rendering::FrameWriter& frame) const {
        auto world = frame.begin_pass(world_camera_.projection_matrix(), world_camera_.view_matrix());
        player_->append(world);
        enemy_->append(world);

        const CE::Rendering::Submission::Context2D context{
            world.semantics(), world.parameters(), world.constraints(), CE::Rendering::Submission::ImageParameter2D{"image", 0}
        };
        CE::Rendering::DrawStyle2D style;
        style.material = images_;
        style.scale = 4;
        style.model_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(80, 340, 0));
        world.add(CE::Rendering::Submission::resolve_sprite(*sprite_, animation_->cell(), style, context));

        auto hud = frame.begin_pass(hud_camera_.projection_matrix(), hud_camera_.view_matrix());
        hud_->append(hud, {24, static_cast<float>(hud_camera_.framebuffer_size().height) - 48});
        controls_->write(frame);
    }
    // [/render]

    // [cleanup]
    void Game::deinit() {
        if (controls_)
            controls_->close_after_quiescence();
        controls_.reset();
        hud_.reset();
        animation_.reset();
        enemy_.reset();
        player_.reset();
        sprite_.reset();
        images_.reset();
        text_.reset();
        for (auto& binding : bindings_) {
            if (binding != 0) {
                static_cast<void>(engine_.input().bindings().unbind(binding));
                binding = 0;
            }
        }
    }
    // [/cleanup]
}
