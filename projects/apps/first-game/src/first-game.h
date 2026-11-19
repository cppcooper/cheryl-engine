#pragma once

#include "level-transition.h"

#include <cheryl/assets/types/2d/sprite.h>
#include <cheryl/assets/types/2d/unicode-text.h>
#include <cheryl/core/controls/input-bindings.h>
#include <cheryl/core/engine/engine-context.h>
#include <cheryl/core/game-framework/abstract-game.h>
#include <cheryl/core/rendering/camera.h>

#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <utility>

namespace FirstGame {
    // [actor]
    struct Actor {
        std::shared_ptr<const CE::Assets::Sprites::Asset> sprite;
        glm::vec2 position;
        CE::Assets::Grids::CellIndex cell = 0;

        Actor(std::shared_ptr<const CE::Assets::Sprites::Asset> resource, glm::vec2 initial_position) noexcept
        : sprite(std::move(resource)), position(initial_position) {}
    };
    // [/actor]

    // [game]
    class Game final : public CE::Game::AbstractGame {
        static constexpr CE::Input::Actions::Id quit_{1};
        static constexpr CE::Input::Actions::Id left_{2};
        static constexpr CE::Input::Actions::Id right_{3};
        static constexpr CE::Input::Actions::Id next_cell_{4};
        static constexpr CE::Input::Actions::Id change_room_{5};
        static constexpr CE::Input::Actions::Id cancel_room_{6};
        static constexpr CE::Input::Actions::Id unload_room_{7};
        static constexpr CE::Input::Actions::Id load_room_{8};
        static constexpr float actor_scale_ = 4.0f;

        CE::Runtime::Context& engine_;
        std::filesystem::path asset_root_;
        CE::Rendering::Camera2D camera_;
        std::array<CE::Input::Actions::BindingId, 8> bindings_{};
        std::optional<Actor> actor_;
        std::optional<CE::Text::Rendering::Generation> score_;
        std::shared_ptr<const CE::Rendering::Resources::Material> images_;
        std::unique_ptr<Levels::Controller> levels_;

        void adopt_current() noexcept;

    public:
        Game(CE::Runtime::Context& engine, std::filesystem::path asset_root);
        void init_platform() override;
        void update(const CE::Game::TickContext& tick) override;
        void prepare_render_frame(CE::Rendering::FrameWriter& frame) const override;
        void deinit_simulation() override;
        void quiesce() override;
        void deinit_platform() override;

        // Serialized simulation observations, not cross-thread live inspection.
        [[nodiscard]] std::shared_ptr<const Levels::ReadyLevel> current_level() const noexcept;
        [[nodiscard]] Levels::Observation transition_observation() const noexcept;
        [[nodiscard]] const std::optional<Actor>& actor() const noexcept { return actor_; }
    };
    // [/game]
}
