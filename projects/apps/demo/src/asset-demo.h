#pragma once

#include <assets/submission/draw2d.h>
#include <core/rendering/render-frame.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace CE::Assets {
    struct ResourceProvider;
}

// Optional application samples: upload on platform, advance on simulation, then
// copy cells/transforms into retained packets without exposing live cursors.
class DemoAssets final {
    struct Character {
        std::shared_ptr<const CE::Assets::Sprite> sprite;
        std::array<CE::Assets::SpriteAnimation, 6> clips;
    };

    std::shared_ptr<const CE::Assets::Tileset> tiles_;
    std::shared_ptr<const CE::Assets::Sprite> weapon_;
    std::optional<Character> character_;
    std::array<std::shared_ptr<const CE::Assets::RenderedText>, 4> labels_;
    std::chrono::duration<double> elapsed_{};
    bool paused_ = false;
    bool visible_ = true;

public:
    void load(const std::filesystem::path& root, CE::Assets::ResourceProvider& provider, const CE::Text::FontCollection& fonts);
    void reset();
    void toggle_pause() { paused_ = !paused_; }
    void toggle_visible() { visible_ = !visible_; }
    void replay_attack();
    void advance(std::chrono::duration<double> delta);
    void write(
        CE::RenderAPIs::RenderPassWriter& pass,
        const CE::RenderAPIs::DrawStyle2D& images,
        const CE::RenderAPIs::DrawStyle2D& text,
        const CE::Assets::SubmissionContext2D& context
    ) const;
    [[nodiscard]] std::string status() const;
};
