#pragma once

#include "controls.h"
#include "enemy.h"
#include "hud.h"
#include "player.h"

#include <cheryl/core/controls/input-bindings.h>
#include <cheryl/core/engine/engine-context.h>
#include <cheryl/core/game-framework/abstract-game.h>
#include <cheryl/core/rendering/camera.h>

#include <array>
#include <filesystem>
#include <memory>
#include <optional>

namespace Tutorial {
    class Game final : public CE::Game::AbstractGame {
        static constexpr CE::Input::Actions::Id quit_{1};
        static constexpr CE::Input::Actions::Id left_{2};
        static constexpr CE::Input::Actions::Id right_{3};
        static constexpr CE::Input::Actions::Id score_{4};
        static constexpr CE::Input::Actions::Id pan_left_{5};
        static constexpr CE::Input::Actions::Id pan_right_{6};

        CE::Runtime::Context& engine_;
        std::filesystem::path asset_root_;
        CE::Rendering::Camera2D world_camera_;
        CE::Rendering::Camera2D hud_camera_;
        glm::vec2 view_origin_{0, 0};
        std::array<CE::Input::Actions::BindingId, 6> bindings_{};
        std::shared_ptr<const CE::Assets::Sprites::Asset> sprite_;
        std::shared_ptr<const CE::Rendering::Resources::Material> images_;
        std::shared_ptr<const CE::Rendering::Resources::Material> text_;
        std::optional<Player> player_;
        std::optional<Enemy> enemy_;
        std::optional<Hud> hud_;
        std::optional<CE::Assets::Sprites::Animation> animation_;
        std::optional<Controls> controls_;
        int points_ = 0;

    public:
        Game(CE::Runtime::Context& engine, std::filesystem::path assets);
        void init() override;
        void update(const CE::Game::TickContext& tick) override;
        void prepare_render_frame(CE::Rendering::FrameWriter& frame) const override;
        void deinit() override;
    };
}
