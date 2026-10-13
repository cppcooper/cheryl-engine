#include "first-game.h"

#include <cheryl/assets/submission/draw2d.h>
#include <cheryl/core/controls/input-interface.h>
#include <cheryl/core/display/window-interface.h>
#include <cheryl/core/resources/asset-management/asset-loader.h>
#include <cheryl/core/resources/asset-management/shader-asset-mgr.h>
#include <cheryl/core/resources/asset-management/sprite-mgr.h>
#include <cheryl/text/font-collection.h>
#include <cheryl/text/layout.h>

#include <glm/gtc/matrix_transform.hpp>
#include <gainput/gainput.h>

#include <algorithm>
#include <stdexcept>

namespace FirstGame {
    Game::Game(CE::Engine::EngineContext& engine, std::filesystem::path asset_root)
    : engine_(engine), asset_root_(std::move(asset_root)) {}

    void Game::init() {
        camera_.set_framebuffer_size(engine_.window().framebuffer_size());

        // [bindings]
        auto& input = engine_.input();
        auto& bindings = input.bindings();
        const auto keyboard = input.keyboard_id();
        bindings_[0] = bindings.bind_button({keyboard, gainput::KeyEscape}, quit_);
        bindings_[1] = bindings.bind_button({keyboard, gainput::KeyLeft}, left_);
        bindings_[2] = bindings.bind_button({keyboard, gainput::KeyRight}, right_);
        bindings_[3] = bindings.bind_button({keyboard, gainput::KeySpace}, next_cell_);
        // [/bindings]

        // [assets]
        auto& resources = engine_.resources();
        CE::Assets::Loader loader(asset_root_);
        loader.load_assets(resources);

        auto sprite = CE::Assets::SpriteMgr::get().get_asset("first-game", "actor");
        images_ = CE::Assets::ShaderAssetMgr::get().get_material("first-game:images");
        text_ = CE::Assets::ShaderAssetMgr::get().get_material("first-game:text");
        if (!sprite || !images_ || !text_)
            throw std::runtime_error("First-game assets require actor, images and text resources");

        const auto size = camera_.framebuffer_size();
        actor_.emplace(std::move(sprite), glm::vec2(static_cast<float>(size.width) * 0.5f, static_cast<float>(size.height) * 0.5f));
        // [/assets]

        // [score]
        CE::Text::FontSelection selection;
        selection.preferred.emplace_back(CE::Text::SystemFontFamily{"DejaVu Sans"});
        selection.automatic_system_fonts = false;
        auto fonts = CE::Text::FontCollection::load(selection);
        auto layout = CE::Text::layout_text(fonts, "Score: 0", {.pixel_height = 24});
        score_.emplace(CE::Assets::upload_text(CE::Assets::prepare_text(std::move(layout)), resources));
        // [/score]
    }

    // [update]
    void Game::update(const CE::GFramework::TickContext& tick) {
        camera_.set_framebuffer_size(tick.framebuffer_size);
        if (tick.input.button(quit_).pressed()) {
            tick.request_stop();
            return;
        }

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
    void Game::prepare_render_frame(CE::RenderAPIs::RenderFrameWriter& frame) const {
        auto pass = frame.begin_pass(camera_.projection_matrix(), camera_.view_matrix());
        const CE::Assets::SubmissionContext2D context{
            pass.semantics(), pass.parameters(), pass.constraints(), CE::Assets::ImageParameter2D{"image", 0}
        };

        CE::RenderAPIs::DrawStyle2D image_style;
        image_style.material = images_;
        image_style.scale = actor_scale_;
        image_style.model_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(actor_->position, 0.0f));
        pass.add(CE::Assets::resolve_sprite(*actor_->sprite, actor_->cell, image_style, context));

        CE::RenderAPIs::DrawStyle2D text_style;
        text_style.material = text_;
        text_style.model_matrix =
            glm::translate(glm::mat4(1.0f), glm::vec3(24.0f, static_cast<float>(camera_.framebuffer_size().height) - 48.0f, 0.0f));
        pass.add(CE::Assets::resolve_text(*score_, text_style, context));
    }
    // [/render]

    // [cleanup]
    void Game::deinit() {
        for (auto& binding : bindings_) {
            if (binding != 0) {
                static_cast<void>(engine_.input().bindings().unbind(binding));
                binding = 0;
            }
        }
        score_.reset();
        actor_.reset();
        images_.reset();
        text_.reset();
    }
    // [/cleanup]
}
