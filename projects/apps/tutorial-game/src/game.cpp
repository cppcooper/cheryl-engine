#include "game.h"

#include <cheryl/assets/resources/resource-provider.h>
#include <cheryl/core/controls/button-codes.h>
#include <cheryl/core/controls/input-interface.h>
#include <cheryl/core/display/window-interface.h>
#include <cheryl/core/resources/asset-management/asset-loader.h>
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

        // [bindings]
        auto& input = engine_.input();
        auto& bindings = input.bindings();
        const auto keyboard = input.keyboard_id();
        keyboard_ = keyboard;
        bindings_[0] = bindings.bind_button({keyboard, CE::Input::KeyboardKey::Escape}, quit_);
        bindings_[1] = bindings.bind_button({keyboard, CE::Input::KeyboardKey::Space}, score_);
        bindings_[2] = bindings.bind_button({keyboard, CE::Input::KeyboardKey::A}, pan_left_);
        bindings_[3] = bindings.bind_button({keyboard, CE::Input::KeyboardKey::D}, pan_right_);
        bindings_[4] = bindings.bind_button({keyboard, CE::Input::KeyboardKey::F2}, join_);
        bindings_[5] = bindings.bind_button({keyboard, CE::Input::KeyboardKey::F3}, leave_);
        player_input_.emplace(input.action_contexts());
        controller_.emplace(player_input_->attach(PlayerController::keyboard(keyboard)));
        // [/bindings]

        // [assets]
        CE::Assets::Loading::Loader loader(asset_root_);
        loader.load_assets(engine_.resources());
        sprite_ = CE::Assets::Sprites::Manager::get().get_asset("first-game", "actor");
        if (!sprite_)
            throw std::runtime_error("Tutorial assets require the actor sprite");
        auto appearance = sprite_->material->definition();
        appearance.sampling["image"] = engine_.resources().create_sampler(
            {.minification = CE::Rendering::Sampling::Filter::Nearest,
             .magnification = CE::Rendering::Sampling::Filter::Nearest,
             .mipmaps = CE::Rendering::Sampling::MipmapFilter::None,
             .anisotropy = CE::Rendering::Sampling::Anisotropy::Disabled}
        );
        images_ = std::make_shared<const CE::Rendering::Resources::Material>(std::move(appearance));
        player_.emplace(sprite_, images_, glm::vec2(160, 100));
        enemy_.emplace(sprite_, images_, glm::vec2(80, 220), 1, 80);
        animation_.emplace(sprite_->animation("blink"));
        // [/assets]

        hud_.emplace(engine_);
    }

    // [update]
    void Game::update(const CE::Game::TickContext& tick) {
        world_camera_.set_framebuffer_size(tick.framebuffer_size);
        if (tick.input.button(quit_).pressed()) {
            tick.request_stop();
            return;
        }
        // [players]
        if (tick.input.button(join_).pressed() && !second_controller_) {
            second_controller_.emplace(
                player_input_->attach(PlayerController::keyboard(keyboard_, CE::Input::KeyboardKey::J, CE::Input::KeyboardKey::L))
            );
            second_player_.emplace(sprite_, images_, glm::vec2(320, 100), 1);
        }
        if (tick.input.button(leave_).pressed()) {
            second_controller_.reset();
            second_player_.reset();
        }
        player_->update(controller_->read(tick.input), tick.delta_seconds);
        if (second_player_)
            second_player_->update(second_controller_->read(tick.input), tick.delta_seconds);
        enemy_->update(patrol_.read(*enemy_, tick.delta_seconds), tick.delta_seconds);
        // [/players]
        animation_->advance(std::chrono::duration<double>(tick.delta_seconds));

        const float pan = (tick.input.button(pan_right_).held() ? 1.0f : 0.0f) - (tick.input.button(pan_left_).held() ? 1.0f : 0.0f);
        view_origin_.x += pan * 100.0f * static_cast<float>(tick.delta_seconds);
        world_camera_.set_view_matrix(glm::translate(glm::mat4(1.0f), glm::vec3(-view_origin_, 0.0f)));

        if (tick.input.button(score_).pressed() && points_ < std::numeric_limits<int>::max())
            hud_->set_score(++points_);
        hud_->update(tick);
    }
    // [/update]

    // [render]
    void Game::prepare_render_frame(CE::Rendering::FrameWriter& frame) const {
        auto world = frame.begin_pass(world_camera_.projection_matrix(), world_camera_.view_matrix());
        player_->append(world);
        if (second_player_)
            second_player_->append(world);
        enemy_->append(world);

        const CE::Rendering::Submission::Context2D context{world.semantics(), world.parameters(), world.constraints()};
        CE::Rendering::DrawStyle2D style;
        style.material = images_;
        style.scale = 4;
        style.model_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(80, 340, 0));
        world.add(CE::Rendering::Submission::resolve_sprite(*sprite_, animation_->cell(), style, context));

        hud_->append(frame);
    }
    // [/render]

    // [cleanup]
    void Game::deinit() {
        if (hud_)
            hud_->close_after_quiescence();
        hud_.reset();
        animation_.reset();
        second_controller_.reset();
        controller_.reset();
        player_input_.reset();
        second_player_.reset();
        enemy_.reset();
        player_.reset();
        sprite_.reset();
        images_.reset();
        for (auto& binding : bindings_) {
            if (binding != 0) {
                static_cast<void>(engine_.input().bindings().unbind(binding));
                binding = 0;
            }
        }
    }
    // [/cleanup]
}
