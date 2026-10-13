#pragma once

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
        std::shared_ptr<const CE::Assets::Sprite> sprite;
        glm::vec2 position;
        CE::Assets::CellIndex cell = 0;

        Actor(std::shared_ptr<const CE::Assets::Sprite> resource, glm::vec2 initial_position)
        : sprite(std::move(resource)), position(initial_position) {}
    };
    // [/actor]

    // [game]
    class Game final : public CE::GFramework::AbstractGame {
        static constexpr CE::Input::ActionId quit_{1};
        static constexpr CE::Input::ActionId left_{2};
        static constexpr CE::Input::ActionId right_{3};
        static constexpr CE::Input::ActionId next_cell_{4};
        static constexpr float actor_scale_ = 4.0f;

        CE::Engine::EngineContext& engine_;
        std::filesystem::path asset_root_;
        CE::Camera2D camera_;
        std::array<CE::Input::BindingId, 4> bindings_{};
        std::optional<Actor> actor_;
        std::optional<CE::Assets::RenderedText> score_;
        std::shared_ptr<const CE::Assets::Material> images_;
        std::shared_ptr<const CE::Assets::Material> text_;

    public:
        Game(CE::Engine::EngineContext& engine, std::filesystem::path asset_root);
        void init() override;
        void update(const CE::GFramework::TickContext& tick) override;
        void prepare_render_frame(CE::RenderAPIs::RenderFrameWriter& frame) const override;
        void deinit() override;
    };
    // [/game]
}
