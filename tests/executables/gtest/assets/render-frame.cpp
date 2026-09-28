#include <gtest/gtest.h>

#include <assets/2d/sprite.h>
#include <core/rendering/render-frame.h>

#include <chrono>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

namespace {
    std::shared_ptr<CE::Assets::Sprite> make_sprite() {
        using namespace std::chrono_literals;
        CE::Assets::SpriteDefinition definition;
        definition.grid.frame = {16, 16};
        definition.grid.rows = 1;
        definition.grid.columns = 4;
        definition.animations.push_back({.name = "walk",
                                         .frames = {{0, 100ms}, {1, 100ms}, {2, 100ms}, {3, 100ms}},
                                         .loop = true});
        definition.animations.push_back({.name = "fall",
                                         .frames = {{0, 100ms}, {1, 100ms}, {2, 100ms}},
                                         .loop = false});
        // Playback and frame publication only inspect metadata; no GPU resources are needed.
        return std::make_shared<CE::Assets::Sprite>(CE::Assets::SpriteData{
            .geometry = nullptr, .texture = nullptr, .definition = std::move(definition)});
    }
}

TEST(sprite_playback, two_entities_share_frames_but_keep_separate_clocks) {
    using namespace std::chrono_literals;
    auto sprite = make_sprite();
    auto first = sprite->animation("walk");
    auto second = sprite->animation("walk");

    first.advance(150ms);
    second.advance(250ms);
    EXPECT_EQ(first.cell(), 1u);
    EXPECT_EQ(second.cell(), 2u);
    EXPECT_EQ(first.definition().frames.data(), second.definition().frames.data());

    first.advance(800ms); // Skip two complete cycles without changing either clip definition.
    EXPECT_EQ(first.cell(), 1u);
    EXPECT_EQ(second.cell(), 2u);
    sprite.reset(); // The playback values retain their definition after the asset is released.
    EXPECT_EQ(first.definition().frames.size(), 4u);
}

TEST(sprite_playback, nonlooping_clip_stays_on_its_last_frame) {
    using namespace std::chrono_literals;
    auto sprite = make_sprite();
    auto fall = sprite->animation("fall");

    fall.advance(5s);
    EXPECT_EQ(fall.cell(), 2u);
    fall.advance(1s);
    EXPECT_EQ(fall.index(), 2u);
    fall[0];
    EXPECT_EQ(fall.cell(), 0u);
}

TEST(render_frame, published_values_do_not_follow_simulation_changes) {
    using namespace std::chrono_literals;
    auto sprite = make_sprite();
    auto playback = sprite->animation("walk");
    playback.advance(150ms);

    glm::mat4 view{1.0f};
    view[3][0] = -40.0f;
    CE::RenderAPIs::RenderPass world;
    world.view = view;
    world.draws.emplace_back(CE::RenderAPIs::SpriteDraw{sprite, playback.cell(), {}});
    std::vector<CE::RenderAPIs::RenderPass> passes;
    passes.push_back(std::move(world));
    const CE::RenderAPIs::RenderFrame frame(std::move(passes));

    playback.advance(100ms);
    view[3][0] = -80.0f;
    sprite.reset();
    const auto& draw = std::get<CE::RenderAPIs::SpriteDraw>(frame.passes()[0].draws[0]);
    EXPECT_EQ(draw.cell, 1u);
    ASSERT_TRUE(draw.sprite);
    EXPECT_FLOAT_EQ(frame.passes()[0].view[3][0], -40.0f);
}
