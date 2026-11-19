#include "first-game.h"

#include <cheryl/assets/submission/draw2d.h>
#include <cheryl/assets/resources/resource-provider.h>
#include <cheryl/core/controls/input-interface.h>
#include <cheryl/core/display/window-interface.h>
#include <cheryl/core/resources/asset-management/asset-loader.h>
#include <cheryl/core/resources/asset-management/sprite-mgr.h>
#include <cheryl/text/font-service.h>
#include <cheryl/text/layout.h>

#include <glm/gtc/matrix_transform.hpp>
#include <cheryl/core/controls/button-codes.h>

#include <algorithm>
#include <stdexcept>

namespace FirstGame {
    Game::Game(CE::Runtime::Context& engine, std::filesystem::path asset_root)
    : engine_(engine), asset_root_(std::move(asset_root)) {}

    void Game::init_platform() {
        camera_.set_framebuffer_size(engine_.window().framebuffer_size());

        // [bindings]
        auto& input = engine_.input();
        auto& bindings = input.bindings();
        const auto keyboard = input.keyboard_id();
        bindings_[0] = bindings.bind_button({keyboard, CE::Input::KeyboardKey::Escape}, quit_);
        bindings_[1] = bindings.bind_button({keyboard, CE::Input::KeyboardKey::Left}, left_);
        bindings_[2] = bindings.bind_button({keyboard, CE::Input::KeyboardKey::Right}, right_);
        bindings_[3] = bindings.bind_button({keyboard, CE::Input::KeyboardKey::Space}, next_cell_);
        bindings_[4] = bindings.bind_button({keyboard, CE::Input::KeyboardKey::Tab}, change_room_);
        bindings_[5] = bindings.bind_button({keyboard, CE::Input::KeyboardKey::Backspace}, cancel_room_);
        bindings_[6] = bindings.bind_button({keyboard, CE::Input::KeyboardKey::U}, unload_room_);
        bindings_[7] = bindings.bind_button({keyboard, CE::Input::KeyboardKey::Enter}, load_room_);
        // [/bindings]

        // [assets]
        auto& resources = engine_.resources();
        CE::Assets::Loading::Loader loader(asset_root_);
        loader.publish_events_to(engine_.events().publisher());
        loader.load_assets(resources);

        auto sprite = CE::Assets::Sprites::Manager::get().get_asset("first-game", "actor");
        if (!sprite)
            throw std::runtime_error("First-game assets require the actor sprite");

        // Keep this small pixel-art image sharp while sharing the engine's pipeline.
        auto appearance = sprite->material->definition();
        appearance.sampling["image"] = resources.create_sampler(
            {.minification = CE::Rendering::Sampling::Filter::Nearest,
             .magnification = CE::Rendering::Sampling::Filter::Nearest,
             .mipmaps = CE::Rendering::Sampling::MipmapFilter::None,
             .anisotropy = CE::Rendering::Sampling::Anisotropy::Disabled}
        );
        images_ = std::make_shared<const CE::Rendering::Resources::Material>(std::move(appearance));
        levels_ = std::make_unique<Levels::Controller>(engine_, std::move(sprite), images_);
        adopt_current();
        // [/assets]

        // [score]
        auto font = engine_.fonts().default_font();
        auto layout = CE::Text::layout_text(font, "Score: 0", {.pixel_height = 24});
        score_.emplace(CE::Text::Rendering::upload(CE::Text::Rendering::prepare(std::move(layout)), resources));
        // [/score]
    }

    void Game::adopt_current() noexcept {
        const auto current = levels_->current();
        if (!current) {
            actor_.reset();
            return;
        }
        const auto size = camera_.framebuffer_size();
        actor_.emplace(current->sprite, current->spawn * glm::vec2(static_cast<float>(size.width), static_cast<float>(size.height)));
        actor_->cell = current->cell;
    }

    // [update]
    void Game::update(const CE::Game::TickContext& tick) {
        camera_.set_framebuffer_size(tick.framebuffer_size);
        if (tick.input.button(quit_).pressed()) {
            tick.request_stop();
            return;
        }

        // Invalidate before polling: even a completed unadopted result is cancellable.
        if (tick.input.button(cancel_room_).pressed())
            static_cast<void>(levels_->cancel(levels_->observation().id));
        if (tick.input.button(unload_room_).pressed()) {
            levels_->unload();
            actor_.reset();
        } else if (tick.input.button(load_room_).pressed() && !levels_->current()) {
            static_cast<void>(levels_->request(Levels::LevelId::Courtyard));
        } else if (tick.input.button(change_room_).pressed()) {
            const auto current = levels_->current();
            if (current)
                static_cast<void>(levels_->request(
                    current->id == Levels::LevelId::Courtyard ? Levels::LevelId::Study : Levels::LevelId::Courtyard
                ));
        }
        if (levels_->advance())
            adopt_current();
        if (!actor_)
            return;

        const float direction = (tick.input.button(right_).held() ? 1.0f : 0.0f) - (tick.input.button(left_).held() ? 1.0f : 0.0f);
        const float width = static_cast<float>(tick.framebuffer_size.width);
        const float half_actor = static_cast<float>(actor_->sprite->definition().grid.frame.width) * actor_scale_ * 0.5f;
        const float margin = std::min(half_actor, width * 0.5f);
        actor_->position.x =
            std::clamp(actor_->position.x + direction * 160.0f * static_cast<float>(tick.delta_seconds), margin, width - margin);
        actor_->position.y = static_cast<float>(tick.framebuffer_size.height) * 0.5f;
        if (tick.input.button(next_cell_).pressed())
            actor_->cell = (actor_->cell + 1) % actor_->sprite->definition().grid.cell_count();
    }
    // [/update]

    // [render]
    void Game::prepare_render_frame(CE::Rendering::FrameWriter& frame) const {
        auto pass = frame.begin_pass(camera_.projection_matrix(), camera_.view_matrix());
        const auto current = current_level();
        if (!current || !actor_)
            return; // An explicitly unloaded room publishes a valid empty pass.
        const CE::Rendering::Submission::Context2D context{pass.semantics(), pass.parameters(), pass.constraints()};

        CE::Rendering::DrawStyle2D image_style;
        image_style.material = images_;
        image_style.scale = actor_scale_;
        image_style.model_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(actor_->position, 0.0f));
        pass.add(CE::Rendering::Submission::resolve_sprite(*actor_->sprite, actor_->cell, image_style, context));

        CE::Rendering::DrawStyle2D text_style;
        text_style.model_matrix =
            glm::translate(glm::mat4(1.0f), glm::vec3(24.0f, static_cast<float>(camera_.framebuffer_size().height) - 48.0f, 0.0f));
        pass.add(CE::Rendering::Submission::resolve_text(*score_, text_style, context));

        text_style.model_matrix =
            glm::translate(glm::mat4(1.0f), glm::vec3(24.0f, static_cast<float>(camera_.framebuffer_size().height) - 80.0f, 0.0f));
        pass.add(CE::Rendering::Submission::resolve_text(current->label, text_style, context));
    }
    // [/render]

    // [cleanup]
    void Game::deinit_simulation() {
        if (levels_)
            levels_->close();
    }

    void Game::quiesce() {
        // Idempotent post-join fallback also covers partial initialization.
        if (levels_)
            levels_->close();
    }

    void Game::deinit_platform() {
        for (auto& binding : bindings_) {
            if (binding != 0) {
                static_cast<void>(engine_.input().bindings().unbind(binding));
                binding = 0;
            }
        }
        levels_.reset();
        score_.reset();
        actor_.reset();
        images_.reset();
    }
    // [/cleanup]

    std::shared_ptr<const Levels::ReadyLevel> Game::current_level() const noexcept {
        return levels_ ? levels_->current() : nullptr;
    }

    Levels::Observation Game::transition_observation() const noexcept {
        return levels_ ? levels_->observation() : Levels::Observation{};
    }
}
