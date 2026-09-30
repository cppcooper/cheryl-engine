#include <gtest/gtest.h>

#include <assets/types/2d/sprite.h>
#include <core/rendering/render-frame.h>

#include <chrono>
#include <memory>
#include <utility>
#include <variant>

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
            .geometry = nullptr,
            .texture = nullptr,
            .definition = std::move(definition)});
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
    glm::mat4 model{1.0f};
    model[3][0] = 5.0f;
    CE::RenderAPIs::RenderFrame frame;
    {
        CE::RenderAPIs::RenderFrameWriter writer(frame);
        auto world = writer.begin_pass(glm::mat4{1.0f}, view);
        CE::RenderAPIs::DrawStyle style;
        style.model_matrix = model;
        world.add(CE::RenderAPIs::SpriteDraw{sprite, playback.cell(), style});
    }

    playback.advance(100ms);
    view[3][0] = -80.0f;
    model[3][0] = 6.0f;
    sprite.reset();
    const auto& draw = std::get<CE::RenderAPIs::SpriteDraw>(frame.passes()[0].draws[0]);
    EXPECT_EQ(draw.cell, 1u);
    ASSERT_TRUE(draw.sprite);
    EXPECT_FLOAT_EQ(frame.passes()[0].view[3][0], -40.0f);
    EXPECT_FLOAT_EQ(draw.style.model_matrix[3][0], 5.0f);
}

TEST(render_frame, recycled_slot_keeps_storage_and_releases_old_assets) {
    auto sprite = make_sprite();
    std::weak_ptr<const CE::Assets::Sprite> retained = sprite;
    CE::RenderAPIs::RenderFrame frame;
    const CE::RenderAPIs::RenderPass* pass_storage = nullptr;
    const CE::RenderAPIs::DrawCommand* draw_storage = nullptr;
    {
        CE::RenderAPIs::RenderFrameWriter writer(frame);
        auto first = writer.begin_pass(glm::mat4{1.0f}, glm::mat4{1.0f});
        first.reserve_draws(2);
        auto second = writer.begin_pass(glm::mat4{1.0f}, glm::mat4{1.0f});
        // Adding another pass may grow the outer vector; the first writer still addresses pass zero.
        first.add(CE::RenderAPIs::SpriteDraw{sprite, 0, {}});
        first.add(CE::RenderAPIs::SpriteDraw{sprite, 1, {}});
        second.add(CE::RenderAPIs::SpriteDraw{sprite, 2, {}});
        ASSERT_EQ(frame.passes().size(), 2u);
        EXPECT_EQ(frame.passes()[0].draws.size(), 2u);
        EXPECT_EQ(frame.passes()[1].draws.size(), 1u);
        pass_storage = frame.passes().data();
        draw_storage = frame.passes()[0].draws.data();
    }

    sprite.reset();
    ASSERT_FALSE(retained.expired());
    frame.recycle(); // Runtime calls this after rendering, while the graphics context is current.
    EXPECT_TRUE(retained.expired());
    EXPECT_TRUE(frame.passes().empty());

    {
        CE::RenderAPIs::RenderFrameWriter writer(frame);
        glm::mat4 next_view{1.0f};
        next_view[3][0] = -6.0f;
        auto first = writer.begin_pass(glm::mat4{1.0f}, next_view);
        first.add(CE::RenderAPIs::SpriteDraw{make_sprite(), 2, {}});
    }
    ASSERT_EQ(frame.passes().size(), 1u);
    EXPECT_EQ(frame.passes().data(), pass_storage);
    EXPECT_EQ(frame.passes()[0].draws.data(), draw_storage);
    EXPECT_EQ(frame.passes()[0].draws.size(), 1u);
    EXPECT_FLOAT_EQ(frame.passes()[0].view[3][0], -6.0f);
}
