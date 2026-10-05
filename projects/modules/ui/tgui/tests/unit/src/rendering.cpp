#include <gtest/gtest.h>

#include <ui/tgui/rendering.h>
#include <internals/exceptions.h>

#include <algorithm>
#include <array>
#include <limits>
#include <memory>

namespace {
    using CE::UI::TGUI::RenderTarget;
    using CE::UI::TGUI::Texture;

    std::array<tgui::Vertex, 4> quad() {
        return {tgui::Vertex{{10, 20}, {255, 0, 0, 128}, {0, 0}}, tgui::Vertex{{60, 20}, {0, 255, 0, 255}, {1, 0}},
                tgui::Vertex{{60, 45}, {0, 0, 255, 128}, {1, 1}}, tgui::Vertex{{10, 45}, {255, 255, 255, 255}, {0, 1}}};
    }

    void configure(RenderTarget& target) {
        target.setView({10, 20, 100, 50}, {20, 30, 200, 100}, {320, 240});
    }

    void draw(RenderTarget& target, const std::shared_ptr<tgui::BackendTexture>& texture = {}) {
        const auto vertices = quad();
        constexpr std::array<unsigned int, 6> indices{0, 1, 2, 0, 2, 3};
        target.drawVertexArray({}, vertices.data(), vertices.size(), indices.data(), indices.size(), texture);
    }
}

TEST(ui_tgui_texture, owned_pixels) {
    Texture texture(32);
    std::array<unsigned char, 8> pixels{255, 0, 0, 255, 0, 0, 255, 128};
    ASSERT_TRUE(texture.loadTextureOnly({1, 2}, pixels.data(), true));
    const auto first = texture.snapshot();
    ASSERT_TRUE(first);
    pixels.fill(0);
    EXPECT_EQ(first->rgba.front(), 255);
    EXPECT_EQ(first->rgba.back(), 128);
    EXPECT_EQ(first->size.height, 2u);
    ASSERT_TRUE(texture.loadTextureOnly({2, 1}, pixels.data(), true));
    EXPECT_NE(texture.snapshot(), first);
    EXPECT_EQ(texture.snapshot()->size.width, 2u);
    EXPECT_EQ(texture.snapshot()->rgba.front(), 0);
    EXPECT_EQ(first->size.width, 1u);
    EXPECT_EQ(first->rgba.front(), 255);
}

TEST(ui_tgui_texture, hit_pixels) {
    Texture texture(32);
    auto pixels = std::make_unique<std::uint8_t[]>(8);
    const std::array<std::uint8_t, 8> colors{255, 255, 255, 0, 255, 255, 255, 255};
    std::copy(colors.begin(), colors.end(), pixels.get());
    ASSERT_TRUE(texture.load({2, 1}, std::move(pixels), true));
    EXPECT_TRUE(texture.isTransparentPixel({0, 0}));
    EXPECT_FALSE(texture.isTransparentPixel({1, 0}));
    EXPECT_EQ(texture.snapshot()->rgba[3], 0);
}

TEST(ui_tgui_texture, limits) {
    EXPECT_THROW(Texture(0), CE::Exceptions::invalid_args);
    EXPECT_THROW(CE::UI::TGUI::Renderer(std::numeric_limits<unsigned int>::max()), CE::Exceptions::invalid_args);
    CE::UI::TGUI::Renderer renderer(32);
    EXPECT_EQ(renderer.getMaximumTextureSize(), 32u);
    EXPECT_TRUE(std::dynamic_pointer_cast<Texture>(renderer.createTexture()));
    Texture texture(1);
    constexpr std::array<unsigned char, 8> pixels{};
    EXPECT_THROW(texture.loadTextureOnly({2, 1}, pixels.data(), true), CE::Exceptions::invalid_args);
    EXPECT_THROW(texture.loadTextureOnly({0, 1}, pixels.data(), true), CE::Exceptions::invalid_args);
    EXPECT_THROW(texture.loadTextureOnly({1, 1}, nullptr, true), CE::Exceptions::invalid_args);
    EXPECT_FALSE(texture.snapshot());
}

TEST(ui_tgui_texture, sampling) {
    Texture texture(32);
    constexpr std::array<unsigned char, 4> pixels{255, 0, 0, 255};
    ASSERT_TRUE(texture.loadTextureOnly({1, 1}, pixels.data(), true));
    const auto saved = texture.snapshot();
    EXPECT_THROW(texture.setSmooth(false), CE::Exceptions::invalid_args);
    EXPECT_THROW(texture.loadTextureOnly({1, 1}, pixels.data(), false), CE::Exceptions::invalid_args);
    EXPECT_EQ(texture.snapshot(), saved);
    EXPECT_TRUE(texture.isSmooth());
    texture.setSmooth(true);
    EXPECT_EQ(texture.snapshot(), saved);
}

TEST(ui_tgui_recording, indices_transform) {
    RenderTarget target;
    configure(target);
    target.begin_recording();
    auto vertices = quad();
    constexpr std::array<unsigned int, 6> indices{2, 0, 1, 2, 1, 3};
    tgui::RenderStates states;
    states.transform.translate({5, 2});
    target.drawVertexArray(states, vertices.data(), vertices.size(), indices.data(), indices.size(), {});
    vertices[2].position = {999, 999};
    const auto scene = target.finish_recording();
    ASSERT_EQ(scene.draws().size(), 1u);
    const auto& copied = scene.draws()[0].vertices;
    ASSERT_EQ(copied.size(), 6u);
    EXPECT_FLOAT_EQ(copied[0].x, 130);
    EXPECT_FLOAT_EQ(copied[0].y, 84);
    EXPECT_FLOAT_EQ(copied[1].x, 30);
    EXPECT_FLOAT_EQ(copied[1].y, 34);
    EXPECT_FLOAT_EQ(copied[0].b, 1);
    EXPECT_FLOAT_EQ(copied[0].a, 128.0f / 255);
    EXPECT_FALSE(scene.draws()[0].texture);
}

TEST(ui_tgui_recording, retained_texture) {
    RenderTarget target;
    configure(target);
    auto texture = std::make_shared<Texture>(32);
    constexpr std::array<unsigned char, 4> red{255, 0, 0, 255};
    constexpr std::array<unsigned char, 4> blue{0, 0, 255, 255};
    ASSERT_TRUE(texture->loadTextureOnly({1, 1}, red.data(), true));
    target.begin_recording();
    draw(target, texture);
    ASSERT_TRUE(texture->loadTextureOnly({1, 1}, blue.data(), true));
    draw(target, texture);
    texture.reset();
    const auto scene = target.finish_recording();
    ASSERT_EQ(scene.draws().size(), 2u);
    EXPECT_EQ(scene.draws()[0].texture->rgba[0], 255);
    EXPECT_EQ(scene.draws()[1].texture->rgba[2], 255);
    EXPECT_NE(scene.draws()[0].texture, scene.draws()[1].texture);
    EXPECT_FLOAT_EQ(scene.draws()[0].vertices[0].v, 1); // TGUI top -> Cheryl image top.
    EXPECT_FLOAT_EQ(scene.draws()[0].vertices[2].v, 0);
}

TEST(ui_tgui_recording, nested_clips) {
    RenderTarget target;
    configure(target);
    target.set_pixel_scale({2, 2});
    EXPECT_EQ(target.getPixelsPerPoint(), tgui::Vector2f(4, 4));
    target.begin_recording();
    target.addClippingLayer({}, {15, 25, 30, 20});
    target.addClippingLayer({}, {25, 30, 30, 20});
    draw(target);
    target.removeClippingLayer();
    draw(target);
    target.removeClippingLayer();
    draw(target);
    const auto scene = target.finish_recording();
    ASSERT_EQ(scene.draws().size(), 3u);
    EXPECT_EQ(scene.draws()[0].clip.rectangle, (CE::RenderAPIs::ClipRect2D{50, 50, 90, 80}));
    EXPECT_EQ(scene.draws()[1].clip.rectangle, (CE::RenderAPIs::ClipRect2D{30, 40, 90, 80}));
    EXPECT_EQ(scene.draws()[2].clip.rectangle, (CE::RenderAPIs::ClipRect2D{20, 30, 220, 130}));
    EXPECT_EQ(
        CE::RenderAPIs::resolve_clip_region(scene.draws()[0].clip, {640, 480}), (CE::RenderAPIs::PixelClipRect2D{100, 100, 180, 160})
    );
    target.setView({0, 0, 640, 480}, {0, 0, 640, 480}, {640, 480});
    EXPECT_EQ(scene.width(), 320);
    EXPECT_EQ(scene.draws()[0].clip.logical_width, 320);
}

TEST(ui_tgui_recording, empty_clip) {
    RenderTarget target;
    configure(target);
    target.begin_recording();
    target.addClippingLayer({}, {200, 300, 20, 30});
    draw(target);
    target.removeClippingLayer();
    target.drawFilledRect({}, {10, 10}, tgui::Color::Red);
    EXPECT_EQ(target.finish_recording().draws().size(), 1u);
    target.setView({}, {}, {0, 0});
    target.begin_recording();
    target.addClippingLayer({}, {});
    draw(target);
    target.removeClippingLayer();
    EXPECT_TRUE(target.finish_recording().draws().empty());
}

TEST(ui_tgui_recording, lifecycle) {
    RenderTarget target;
    EXPECT_THROW(target.begin_recording(), CE::Exceptions::failed_operation);
    configure(target);
    EXPECT_THROW(draw(target), CE::Exceptions::failed_operation);
    target.begin_recording();
    EXPECT_THROW(target.begin_recording(), CE::Exceptions::failed_operation);
    EXPECT_THROW(configure(target), CE::Exceptions::failed_operation);
    EXPECT_THROW(target.set_pixel_scale({2, 2}), CE::Exceptions::failed_operation);
    target.addClippingLayer({}, {10, 20, 10, 10});
    EXPECT_THROW(target.finish_recording(), CE::Exceptions::failed_operation);
    target.discard_recording();
    target.begin_recording();
    EXPECT_THROW(target.removeClippingLayer(), CE::Exceptions::failed_operation);
    draw(target);
    EXPECT_EQ(target.finish_recording().draws().size(), 1u);
    EXPECT_THROW(target.clearScreen(), CE::Exceptions::failed_operation);
}

TEST(ui_tgui_recording, invalid_draw) {
    RenderTarget target;
    configure(target);
    target.begin_recording();
    draw(target);
    auto vertices = quad();
    constexpr std::array<unsigned int, 3> bad{0, 1, 4};
    EXPECT_THROW(target.drawVertexArray({}, vertices.data(), vertices.size(), bad.data(), bad.size(), {}), CE::Exceptions::invalid_args);
    EXPECT_THROW(target.drawVertexArray({}, vertices.data(), vertices.size(), bad.data(), 2, {}), CE::Exceptions::invalid_args);
    constexpr std::array<unsigned int, 3> indices{0, 1, 2};
    vertices[0].position.x = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(
        target.drawVertexArray({}, vertices.data(), vertices.size(), indices.data(), indices.size(), {}), CE::Exceptions::invalid_args
    );
    EXPECT_EQ(target.finish_recording().draws().size(), 1u);
}

TEST(ui_tgui_recording, invalid_view) {
    RenderTarget target;
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(target.setView({0, 0, -1, 1}, {}, {320, 240}), CE::Exceptions::invalid_args);
    EXPECT_THROW(target.setView({0, 0, 1, 1}, {}, {nan, 240}), CE::Exceptions::invalid_args);
    EXPECT_THROW(target.set_pixel_scale({0, 1}), CE::Exceptions::invalid_args);
    configure(target);
    target.begin_recording();
    tgui::RenderStates rotated;
    rotated.transform.rotate(45);
    EXPECT_THROW(target.addClippingLayer(rotated, {10, 20, 10, 10}), CE::Exceptions::invalid_args);
    draw(target);
    EXPECT_EQ(target.finish_recording().draws().size(), 1u);
}
