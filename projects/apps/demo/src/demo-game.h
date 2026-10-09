#pragma once

#include <core/game-framework/abstract-game.h>
#include <core/game-framework/tick-context.h>
#include <core/rendering/render-frame.h>
#include <text/font-collection.h>
#include <text/layout.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>

namespace CE::Engine {
    class EngineContext;
}

// The engine-facing application object. main.cpp owns startup and runtime;
// this class owns the game lifecycle that the runtime drives.
class DemoGame final : public CE::GFramework::AbstractGame {
    struct GameState;
    std::unique_ptr<GameState> game_state_;

public:
    DemoGame(
        CE::Engine::EngineContext& engine,
        std::filesystem::path asset_root,
        bool load_all_assets,
        CE::Text::FontSelection fonts,
        CE::Text::LayoutOptions text_options,
        bool unicode_preview
    );
    ~DemoGame() override;
    DemoGame(const DemoGame&) = delete;
    DemoGame& operator=(const DemoGame&) = delete;

    void stop_after_updates(std::uint64_t count, std::function<void()> stop);
    [[nodiscard]] std::uint64_t completed_updates() const;

    void init() override;
    void deinit() override;
    void update(const CE::GFramework::TickContext& tick) override;
    void prepare_render_frame(CE::RenderAPIs::RenderFrameWriter& frame) const override;
};
