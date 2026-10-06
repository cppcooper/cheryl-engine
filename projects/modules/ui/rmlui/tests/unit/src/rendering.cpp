#include <ui/rmlui/rendering.h>
#include <internals/exceptions.h>

#include <RmlUi/Core/Matrix4.h>
#include <gtest/gtest.h>

#include <array>
#include <limits>

namespace {
    using namespace CE::UI::RmlUi;

    std::array<Rml::Vertex, 3> triangle() {
        return {Rml::Vertex{{0, 0}, {128, 0, 0, 128}, {0, 0}}, Rml::Vertex{{20, 0}, {0, 255, 0, 255}, {1, 0}},
                Rml::Vertex{{0, 20}, {0, 0, 128, 128}, {0, 1}}};
    }
    Rml::CompiledGeometryHandle compile(RenderTarget& renderer) {
        const auto vertices = triangle();
        constexpr std::array<int, 3> indices{0, 1, 2};
        return renderer.CompileGeometry({vertices.data(), vertices.size()}, {indices.data(), indices.size()});
    }
}

TEST(ui_rmlui_recording, owned_data) {
    RenderTarget renderer;
    auto vertices = triangle();
    constexpr std::array<int, 3> indices{2, 0, 1};
    const auto geometry = renderer.CompileGeometry({vertices.data(), vertices.size()}, {indices.data(), indices.size()});
    std::array<unsigned char, 8> pixels{128, 0, 0, 128, 0, 0, 255, 255};
    const auto texture = renderer.GenerateTexture({pixels.data(), pixels.size()}, {1, 2});
    vertices.fill({});
    pixels.fill(0);
    renderer.set_view({320, 240});
    renderer.begin_recording();
    renderer.RenderGeometry(geometry, {5, 7}, texture);
    renderer.ReleaseGeometry(geometry);
    renderer.ReleaseTexture(texture);
    const auto scene = renderer.finish_recording();
    ASSERT_EQ(scene.draws().size(), 1u);
    const auto& draw = scene.draws()[0];
    ASSERT_EQ(draw.vertices.size(), 3u);
    EXPECT_FLOAT_EQ(draw.vertices[0].x, 5);
    EXPECT_FLOAT_EQ(draw.vertices[0].y, 27);
    EXPECT_FLOAT_EQ(draw.vertices[0].b, 128.0f / 255);
    EXPECT_FLOAT_EQ(draw.vertices[0].a, 128.0f / 255);
    EXPECT_FLOAT_EQ(draw.vertices[0].v, 0);
    EXPECT_FLOAT_EQ(draw.vertices[1].v, 1);
    ASSERT_TRUE(draw.texture);
    EXPECT_EQ(draw.texture->rgba, (std::vector<unsigned char>{128, 0, 0, 128, 0, 0, 255, 255}));
}

TEST(ui_rmlui_recording, clips) {
    RenderTarget renderer;
    const auto geometry = compile(renderer);
    renderer.set_view({320, 240});
    renderer.begin_recording();
    renderer.EnableScissorRegion(true);
    renderer.SetScissorRegion(Rml::Rectanglei::FromCorners({10, 20}, {100, 80}));
    renderer.RenderGeometry(geometry, {}, 0);
    renderer.SetScissorRegion(Rml::Rectanglei::FromCorners({400, 20}, {500, 80}));
    renderer.RenderGeometry(geometry, {}, 0);
    renderer.EnableScissorRegion(false);
    renderer.RenderGeometry(geometry, {}, 0);
    const auto scene = renderer.finish_recording();
    ASSERT_EQ(scene.draws().size(), 2u);
    EXPECT_EQ(scene.draws()[0].clip.rectangle, (CE::RenderAPIs::ClipRect2D{10, 20, 100, 80}));
    EXPECT_EQ(CE::RenderAPIs::resolve_clip_region(scene.draws()[0].clip, {640, 480}), (CE::RenderAPIs::PixelClipRect2D{20, 40, 200, 160}));
    EXPECT_EQ(scene.draws()[1].clip.rectangle, (CE::RenderAPIs::ClipRect2D{0, 0, 320, 240}));
    renderer.set_view({640, 480});
    EXPECT_EQ(scene.draws()[0].clip.logical_width, 320);
}

TEST(ui_rmlui_texture, file_alpha) {
    RenderTarget renderer;
    Rml::Vector2i dimensions;
    const auto texture = renderer.LoadTexture(dimensions, CHERYL_RMLUI_TEST_IMAGE);
    ASSERT_NE(texture, 0u);
    EXPECT_EQ(dimensions, (Rml::Vector2i{1, 2}));
    renderer.set_view({320, 240});
    renderer.begin_recording();
    renderer.RenderGeometry(compile(renderer), {}, texture);
    const auto scene = renderer.finish_recording();
    ASSERT_EQ(scene.draws().size(), 1u);
    // Decoder storage remains top-to-bottom. Only straight file pixels undergo
    // premultiplication; generated atlas pixels already have the correct alpha.
    EXPECT_EQ(scene.draws()[0].texture->rgba, (std::vector<unsigned char>{128, 0, 0, 128, 0, 0, 255, 255}));
    EXPECT_EQ(renderer.LoadTexture(dimensions, "absent-rmlui-image.png"), 0u);
    EXPECT_EQ(dimensions, Rml::Vector2i{});
}

TEST(ui_rmlui_texture, limits) {
    EXPECT_THROW(RenderTarget(0), CE::Exceptions::invalid_args);
    EXPECT_THROW(RenderTarget(std::numeric_limits<unsigned int>::max()), CE::Exceptions::invalid_args);
    RenderTarget renderer(1);
    constexpr std::array<unsigned char, 8> pixels{};
    EXPECT_THROW(renderer.GenerateTexture({pixels.data(), pixels.size()}, {2, 1}), CE::Exceptions::invalid_args);
    EXPECT_THROW(renderer.GenerateTexture({pixels.data(), pixels.size()}, {1, 1}), CE::Exceptions::invalid_args);
    EXPECT_THROW(renderer.GenerateTexture({}, {0, 1}), CE::Exceptions::invalid_args);
}

TEST(ui_rmlui_recording, lifecycle) {
    RenderTarget renderer;
    const auto geometry = compile(renderer);
    EXPECT_THROW(renderer.begin_recording(), CE::Exceptions::failed_operation);
    EXPECT_THROW(renderer.RenderGeometry(geometry, {}, 0), CE::Exceptions::failed_operation);
    renderer.set_view({320, 240});
    renderer.begin_recording();
    EXPECT_THROW(renderer.begin_recording(), CE::Exceptions::failed_operation);
    EXPECT_THROW(renderer.set_view({640, 480}), CE::Exceptions::failed_operation);
    renderer.RenderGeometry(geometry, {}, 0);
    renderer.discard_recording();
    renderer.begin_recording();
    EXPECT_TRUE(renderer.finish_recording().draws().empty());
    EXPECT_THROW(static_cast<void>(renderer.finish_recording()), CE::Exceptions::failed_operation);
    renderer.set_view({0, 0});
    renderer.begin_recording();
    renderer.RenderGeometry(geometry, {}, 0);
    EXPECT_TRUE(renderer.finish_recording().draws().empty());
}

TEST(ui_rmlui_recording, invalid_data) {
    RenderTarget renderer;
    auto vertices = triangle();
    constexpr std::array<int, 3> bad{0, 1, 3};
    EXPECT_THROW(renderer.CompileGeometry({vertices.data(), vertices.size()}, {bad.data(), bad.size()}), CE::Exceptions::invalid_args);
    constexpr std::array<int, 3> good{0, 1, 2};
    vertices[0].position.x = std::numeric_limits<float>::quiet_NaN();
    EXPECT_THROW(renderer.CompileGeometry({vertices.data(), vertices.size()}, {good.data(), good.size()}), CE::Exceptions::invalid_args);
    renderer.set_view({320, 240});
    renderer.begin_recording();
    const auto geometry = compile(renderer);
    EXPECT_THROW(renderer.RenderGeometry(geometry, {}, 42), CE::Exceptions::invalid_args);
    renderer.ReleaseGeometry(geometry);
    EXPECT_THROW(renderer.RenderGeometry(geometry, {}, 0), CE::Exceptions::invalid_args);
    EXPECT_TRUE(renderer.finish_recording().draws().empty());
}

TEST(ui_rmlui_recording, unsupported) {
    RenderTarget renderer;
    renderer.set_view({320, 240});
    renderer.begin_recording();
    renderer.RenderGeometry(compile(renderer), {}, 0);
    EXPECT_EQ(renderer.PushLayer(), 0u);
    EXPECT_NO_THROW(renderer.PopLayer());
    EXPECT_THROW(static_cast<void>(renderer.finish_recording()), CE::Exceptions::failed_operation);
    // An unsupported compiled property may not be offered again by the SDK.
    // This session remains rejected rather than publishing a later partial frame.
    EXPECT_THROW(renderer.begin_recording(), CE::Exceptions::failed_operation);
}

TEST(ui_rmlui_recording, transforms) {
    RenderTarget renderer;
    renderer.set_view({320, 240});
    renderer.begin_recording();
    const auto identity = Rml::Matrix4f::Identity();
    renderer.SetTransform(nullptr);
    renderer.SetTransform(&identity);
    EXPECT_NO_THROW(static_cast<void>(renderer.finish_recording()));
    renderer.begin_recording();
    const auto transform = Rml::Matrix4f::Translate(1, 2, 0);
    renderer.SetTransform(&transform);
    EXPECT_THROW(static_cast<void>(renderer.finish_recording()), CE::Exceptions::failed_operation);
}
