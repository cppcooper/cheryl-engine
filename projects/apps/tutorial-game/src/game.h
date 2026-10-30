#pragma once

#include "character.h"
#include "patrol-controller.h"
#include "player-controller.h"
#include "ui-controller.h"

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
        static constexpr CE::Input::Actions::Id score_{4};
        static constexpr CE::Input::Actions::Id pan_left_{5};
        static constexpr CE::Input::Actions::Id pan_right_{6};
        static constexpr CE::Input::Actions::Id join_{7};
        static constexpr CE::Input::Actions::Id leave_{8};

        CE::Runtime::Context& engine_;
        std::filesystem::path asset_root_;
        CE::Rendering::Camera2D world_camera_;
        glm::vec2 view_origin_{0, 0};
        std::array<CE::Input::Actions::BindingId, 6> bindings_{};
        CE::Input::DeviceId keyboard_ = 0;
        std::shared_ptr<const CE::Assets::Sprites::Asset> sprite_;
        std::shared_ptr<const CE::Rendering::Resources::Material> images_;
        std::optional<CE::Input::Actions::ContextSubmission> player_input_;
        std::optional<PlayerController> controller_;
        std::optional<PlayerController> second_controller_;
        std::optional<Character> player_;
        std::optional<Character> second_player_;
        std::optional<Character> enemy_;
        PatrolController patrol_{80, 500};
        std::optional<UIController> ui_;
        std::optional<CE::Assets::Sprites::Animation> animation_;
        int points_ = 0;

    public:
        Game(CE::Runtime::Context& engine, std::filesystem::path assets);
        void init_platform() override;
        void init_simulation() override;
        void update(const CE::Game::TickContext& tick) override;
        void prepare_render_frame(CE::Rendering::FrameWriter& frame) const override;
        void deinit_platform() override;
    };
}
