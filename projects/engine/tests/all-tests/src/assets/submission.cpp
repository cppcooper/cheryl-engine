#include <assets/submission/draw2d.h>
#include <assets/types/2d/stbfont.h>
#include <assets/types/2d/ffont.h>
#include <gtest/gtest.h>
#include <internals/exceptions.h>

#include <array>
#include <chrono>
#include <memory>
#include <utility>

#ifdef GL_VERSION_3_3
#error Asset submission must not include OpenGL.
#endif

namespace {

    struct SubmissionGeometry final : CE::Rendering::Geometry::Buffer2D {
        std::size_t count;
        CE::Rendering::Geometry::Topology uploaded;
        mutable int native_calls = 0;

        SubmissionGeometry(std::size_t count, CE::Rendering::Geometry::Topology topology)
        : count(count), uploaded(topology) {}
        CE::Rendering::Geometry::Layout2D vertex_layout() const noexcept override {
            return CE::Rendering::Geometry::Layout2D::Position3UV2;
        }
        CE::Rendering::Geometry::Topology topology() const noexcept override { return uploaded; }
        std::size_t vertex_count() const noexcept override { return count; }
        void bind() const override { ++native_calls; }
        void draw(std::size_t, std::size_t) const override { ++native_calls; }
    };
    struct SubmissionImage final : CE::Rendering::Resources::Image {
        mutable int native_calls = 0;
        CE::Rendering::Geometry::PixelSize pixel_size() const override { return {16, 16}; }
        void bind(std::uint32_t) const override { ++native_calls; }
    };
    class SubmissionPipeline final : public CE::Rendering::Resources::Pipeline {
    public:
        explicit SubmissionPipeline(CE::Rendering::Geometry::Topology topology)
        : CE::Rendering::Resources::Pipeline(make_definition(topology)) {}

    private:
        static CE::Rendering::Pipelines::Definition make_definition(CE::Rendering::Geometry::Topology topology) {
            CE::Rendering::Pipelines::Definition definition;
            definition.program_sources = {"submission.vert", "submission.frag"};
            definition.topology = topology;
            definition.parameters = {
                {"model", CE::Rendering::Parameters::Type::Mat4, true, CE::Rendering::Parameters::Semantic::Model},
                {"projection", CE::Rendering::Parameters::Type::Mat4, true, CE::Rendering::Parameters::Semantic::Projection},
                {"alpha", CE::Rendering::Parameters::Type::Float, true, CE::Rendering::Parameters::Semantic::Alpha},
                {"scale", CE::Rendering::Parameters::Type::Float, true, CE::Rendering::Parameters::Semantic::Scale},
                {"image", CE::Rendering::Parameters::Type::Sampler2D}
            };
            return definition;
        }
    };
    CE::Rendering::DrawStyle2D make_style(CE::Rendering::Geometry::Topology topology) {
        CE::Rendering::DrawStyle2D style;
        style.material = std::make_shared<CE::Rendering::Resources::Material>(
            CE::Rendering::Materials::Definition{std::make_shared<SubmissionPipeline>(topology), {}}
        );
        return style;
    }
    CE::Rendering::Submission::Context2D make_context() {
        CE::Rendering::Submission::Context2D context;
        context.image = CE::Rendering::Submission::ImageParameter2D{"image", 2};
        context.pass.projection[3][0] = 19.0f;
        return context;
    }
}

TEST(asset_submission, sprite_resource_retention) {
    auto geometry = std::make_shared<SubmissionGeometry>(8, CE::Rendering::Geometry::Topology::TriangleStrip);
    auto image = std::make_shared<SubmissionImage>();
    CE::Assets::Sprites::Definition definition;
    definition.grid.frame = {16, 16};
    definition.grid.rows = 1;
    definition.grid.columns = 2;
    auto sprite = std::make_shared<CE::Assets::Sprites::Asset>(CE::Assets::Sprites::Data{geometry, image, definition});
    std::weak_ptr<CE::Assets::Sprites::Asset> old_asset = sprite;
    auto style = make_style(CE::Rendering::Geometry::Topology::TriangleStrip);
    style.model_matrix[3][0] = 5.0f;
    auto context = make_context();
    const auto packet = CE::Rendering::Submission::resolve_sprite(*sprite, 1, style, context);
    style.model_matrix[3][0] = 90.0f;
    context.pass.projection[3][0] = 70.0f;
    sprite.reset();
    EXPECT_TRUE(old_asset.expired());
    EXPECT_EQ(packet.geometry, geometry);
    EXPECT_EQ(packet.first_vertex, 4u);
    EXPECT_EQ(packet.vertex_count, 4u);
    EXPECT_FLOAT_EQ(std::get<glm::mat4>(packet.parameters.at("model"))[3][0], 5.0f);
    EXPECT_FLOAT_EQ(std::get<glm::mat4>(packet.parameters.at("projection"))[3][0], 19.0f);
    EXPECT_EQ(std::get<CE::Rendering::Parameters::ImageBinding>(packet.parameters.at("image")).image, image);
    EXPECT_EQ(std::get<CE::Rendering::Parameters::ImageBinding>(packet.parameters.at("image")).unit, 2u);
    EXPECT_EQ(geometry->native_calls, 0);
    EXPECT_EQ(image->native_calls, 0);
}

TEST(asset_submission, text_layout_and_retention) {
    auto geometry =
        std::make_shared<SubmissionGeometry>(CE::Text::Legacy::ascii_character_count * 6, CE::Rendering::Geometry::Topology::Triangles);
    auto image = std::make_shared<SubmissionImage>();
    std::array<float, CE::Text::Legacy::ascii_character_count> advances;
    advances.fill(5.0f);
    auto font = std::make_shared<CE::Text::Legacy::AsciiFont>(CE::Text::Legacy::AsciiFontData{geometry, image, advances, 12.0f});
    std::weak_ptr<CE::Text::Legacy::AsciiFont> old_font = font;
    auto style = make_style(CE::Rendering::Geometry::Topology::Triangles);
    style.model_matrix[3][0] = 10.0f;
    style.scale = 2.0f;
    const auto packets = CE::Rendering::Submission::resolve_text(*font, "A \n\tZ\xc3", style, make_context());
    font.reset();
    EXPECT_TRUE(old_font.expired());
    ASSERT_EQ(packets.size(), 3u);
    EXPECT_EQ(packets[0].first_vertex, static_cast<std::size_t>('A' - CE::Text::Legacy::ascii_first_character) * 6);
    EXPECT_EQ(packets[1].first_vertex, static_cast<std::size_t>('Z' - CE::Text::Legacy::ascii_first_character) * 6);
    EXPECT_EQ(packets[2].first_vertex, static_cast<std::size_t>('?' - CE::Text::Legacy::ascii_first_character) * 6);
    const auto model = std::get<glm::mat4>(packets[1].parameters.at("model"));
    EXPECT_FLOAT_EQ(model[3][0], 50.0f);
    EXPECT_FLOAT_EQ(model[3][1], -24.0f);
    EXPECT_FLOAT_EQ(std::get<float>(packets[1].parameters.at("scale")), 2.0f);
    EXPECT_EQ(packets[1].geometry, geometry);
    EXPECT_EQ(geometry->native_calls, 0);
    EXPECT_EQ(image->native_calls, 0);
}

TEST(asset_submission, utf8_text) {
    auto geometry =
        std::make_shared<SubmissionGeometry>(CE::Text::Legacy::ascii_character_count * 6, CE::Rendering::Geometry::Topology::Triangles);
    auto image = std::make_shared<SubmissionImage>();
    std::weak_ptr<SubmissionGeometry> retained_geometry = geometry;
    std::weak_ptr<SubmissionImage> retained_image = image;
    std::array<float, CE::Text::Legacy::ascii_character_count> advances;
    advances.fill(5.0f);
    auto font = std::make_unique<CE::Text::Legacy::AsciiFont>(CE::Text::Legacy::AsciiFontData{geometry, image, advances, 12.0f});
    auto style = make_style(CE::Rendering::Geometry::Topology::Triangles);
    style.model_matrix[3][0] = 10.0f;
    style.scale = 2.0f;
    std::string text = "A\xc3\xa9\xe4\xb8\xad\xf0\x9f\x98\x80" "B";
    const auto packets = CE::Rendering::Submission::resolve_text(*font, text, style, make_context());
    text.clear();
    font.reset();
    geometry.reset();
    image.reset();
    ASSERT_EQ(packets.size(), 5u);
    EXPECT_EQ(packets.front().first_vertex, static_cast<std::size_t>('A' - CE::Text::Legacy::ascii_first_character) * 6);
    for (std::size_t index = 1; index < 4; ++index)
        EXPECT_EQ(packets[index].first_vertex, static_cast<std::size_t>('?' - CE::Text::Legacy::ascii_first_character) * 6);
    EXPECT_EQ(packets.back().first_vertex, static_cast<std::size_t>('B' - CE::Text::Legacy::ascii_first_character) * 6);
    EXPECT_FLOAT_EQ(std::get<glm::mat4>(packets.back().parameters.at("model"))[3][0], 50.0f);
    ASSERT_FALSE(retained_geometry.expired());
    ASSERT_FALSE(retained_image.expired());
    EXPECT_EQ(packets.front().geometry, retained_geometry.lock());
    EXPECT_EQ(std::get<CE::Rendering::Parameters::ImageBinding>(packets.back().parameters.at("image")).image, retained_image.lock());
    EXPECT_EQ(retained_geometry.lock()->native_calls, 0);
    EXPECT_EQ(retained_image.lock()->native_calls, 0);
}

TEST(font_layout, independent_font_banks) {
    auto geometry =
        std::make_shared<SubmissionGeometry>(CE::Text::Legacy::bitmap_character_count * 6, CE::Rendering::Geometry::Topology::Triangles);
    auto image = std::make_shared<SubmissionImage>();
    std::array<float, CE::Text::Legacy::bitmap_character_count> widths;
    widths.fill(128.0f);
    widths['A' - 32] = 64.0f;
    CE::Text::Legacy::BitmapFont font(CE::Text::Legacy::BitmapFontData{widths, geometry, image});
    const auto plain = font.layout("AB");
    const auto fancy = font.layout("AB", {true});
    ASSERT_EQ(plain.size(), 2u);
    ASSERT_EQ(fancy.size(), 2u);
    EXPECT_EQ(plain[0].index, static_cast<std::size_t>('A' - 32));
    EXPECT_EQ(fancy[0].index, plain[0].index + 128);
    EXPECT_FLOAT_EQ(plain[1].x, 0.5f);
    EXPECT_FLOAT_EQ(fancy[1].x, 1.0f);
    EXPECT_EQ(geometry->native_calls, 0);
    EXPECT_EQ(image->native_calls, 0);
}

TEST(asset_submission, legacy_text_transform) {
    auto geometry =
        std::make_shared<SubmissionGeometry>(CE::Text::Legacy::bitmap_character_count * 6, CE::Rendering::Geometry::Topology::Triangles);
    auto image = std::make_shared<SubmissionImage>();
    std::array<float, CE::Text::Legacy::bitmap_character_count> widths;
    widths.fill(128.0f);
    widths['A' - 32] = 64.0f;
    CE::Text::Legacy::BitmapFont font(CE::Text::Legacy::BitmapFontData{widths, geometry, image});
    auto style = make_style(CE::Rendering::Geometry::Topology::Triangles);
    style.scale = 2.0f;
    // A quarter-turn around (10, 20): local +x becomes world +y, and
    // local -y becomes world +x. Every line uses this same caller transform.
    style.model_matrix[0] = glm::vec4{0.0f, 1.0f, 0.0f, 0.0f};
    style.model_matrix[1] = glm::vec4{-1.0f, 0.0f, 0.0f, 0.0f};
    style.model_matrix[3] = glm::vec4{10.0f, 20.0f, 0.0f, 1.0f};
    const auto packets = CE::Rendering::Submission::resolve_text(font, "A A\nA\r\x01\xff", style, make_context());
    ASSERT_EQ(packets.size(), 5u);
    const auto first = std::get<glm::mat4>(packets[0].parameters.at("model"));
    const auto after_space = std::get<glm::mat4>(packets[1].parameters.at("model"));
    const auto new_line = std::get<glm::mat4>(packets[2].parameters.at("model"));
    const auto control_fallback = std::get<glm::mat4>(packets[3].parameters.at("model"));
    const auto high_byte_fallback = std::get<glm::mat4>(packets[4].parameters.at("model"));
    EXPECT_FLOAT_EQ(first[3][0], 10.0f);
    EXPECT_FLOAT_EQ(first[3][1], 20.0f);
    EXPECT_FLOAT_EQ(after_space[3][0], 10.0f);
    EXPECT_FLOAT_EQ(after_space[3][1], 23.0f);
    EXPECT_FLOAT_EQ(new_line[3][0], 10.0f + 2.0f / 128.0f);
    EXPECT_FLOAT_EQ(new_line[3][1], 20.0f);
    EXPECT_FLOAT_EQ(control_fallback[3][1], 21.0f);
    EXPECT_FLOAT_EQ(high_byte_fallback[3][1], 23.0f);
    for (const auto& packet : packets) {
        EXPECT_EQ(packet.vertex_count, 6u);
        EXPECT_EQ(packet.geometry, geometry);
        EXPECT_EQ(std::get<CE::Rendering::Parameters::ImageBinding>(packet.parameters.at("image")).image, image);
        EXPECT_FLOAT_EQ(std::get<float>(packet.parameters.at("scale")), 2.0f);
        const auto model = std::get<glm::mat4>(packet.parameters.at("model"));
        EXPECT_FLOAT_EQ(model[0][0], 0.0f);
        EXPECT_FLOAT_EQ(model[0][1], 1.0f);
        EXPECT_FLOAT_EQ(model[1][0], -1.0f);
    }
    EXPECT_EQ(packets[3].first_vertex, static_cast<std::size_t>('?' - 32) * 6);
    EXPECT_EQ(packets[4].first_vertex, packets[3].first_vertex);
    EXPECT_EQ(geometry->native_calls, 0);
    EXPECT_EQ(image->native_calls, 0);
}

TEST(asset_submission, tile_strip_ranges) {
    using namespace std::chrono_literals;
    auto geometry = std::make_shared<SubmissionGeometry>(16, CE::Rendering::Geometry::Topology::TriangleStrip);
    auto image = std::make_shared<SubmissionImage>();
    CE::Assets::Tilesets::Definition definition;
    definition.grid.frame = {16, 16};
    definition.grid.rows = 1;
    definition.grid.columns = 4;
    CE::Assets::Tilesets::Asset tileset(CE::Assets::Tilesets::Data{geometry, image, definition});
    const auto style = make_style(CE::Rendering::Geometry::Topology::TriangleStrip);
    const auto context = make_context();
    const auto grid = CE::Rendering::Submission::resolve_tile(tileset, 3, style, context);
    const auto tile = CE::Rendering::Submission::resolve_tile(tileset.tile(3), style, context);
    EXPECT_EQ(grid.first_vertex, 12u);
    EXPECT_EQ(tile.first_vertex, grid.first_vertex);
    CE::Assets::Tilesets::Clip clip;
    clip.frames = {{0, 100ms}, {2, 100ms}};
    CE::Assets::Tilesets::Animation animation(clip, geometry, image);
    animation[1];
    const auto animated = CE::Rendering::Submission::resolve_tile(animation, style, context);
    animation[0];
    EXPECT_EQ(animated.first_vertex, 8u);
    EXPECT_EQ(animated.vertex_count, 4u);
    EXPECT_EQ(std::get<CE::Rendering::Parameters::ImageBinding>(animated.parameters.at("image")).image, image);
    EXPECT_THROW(static_cast<void>(CE::Rendering::Submission::resolve_tile(tileset, 4, style, context)), CE::Exceptions::invalid_args);
    EXPECT_EQ(geometry->native_calls, 0);
    EXPECT_EQ(image->native_calls, 0);
}

TEST(asset_submission, selected_tile) {
    using namespace std::chrono_literals;
    auto geometry = std::make_shared<SubmissionGeometry>(16, CE::Rendering::Geometry::Topology::TriangleStrip);
    auto image = std::make_shared<SubmissionImage>();
    std::weak_ptr<SubmissionGeometry> retained_geometry = geometry;
    std::weak_ptr<SubmissionImage> retained_image = image;
    CE::Assets::Tilesets::Definition definition;
    definition.grid.rows = 1;
    definition.grid.columns = 4;
    CE::Assets::Tilesets::BitmaskRule rule;
    rule.type = CE::Assets::Tilesets::BitmaskType::FourNeighbor;
    rule.bit_order = {CE::Assets::Tilesets::Direction::North};
    rule.cases.emplace(0, 3);
    definition.autotiles.emplace("border", rule);
    CE::Assets::Tilesets::Clip clip;
    clip.target = 3;
    clip.frames = {{1, 30ms}, {2, 50ms}};
    definition.animations.emplace("water", clip);
    auto tileset = std::make_unique<CE::Assets::Tilesets::Asset>(CE::Assets::Tilesets::Data{geometry, image, std::move(definition)});
    std::size_t samples = 0;
    const CE::Assets::Tilesets::Selection::Sampler sampler = [&](CE::Assets::Tilesets::Selection::Site) {
        ++samples;
        return CE::Assets::Tilesets::Selection::Sample{CE::Assets::Tilesets::Selection::SampleState::Outside};
    };
    const auto selection = tileset->select_tile("border", sampler, {}, 30ms);
    ASSERT_TRUE(std::holds_alternative<CE::Assets::Grids::CellIndex>(selection));
    const auto packet = CE::Rendering::Submission::resolve_tile(
        *tileset, std::get<CE::Assets::Grids::CellIndex>(selection), make_style(CE::Rendering::Geometry::Topology::TriangleStrip),
        make_context()
    );
    tileset.reset();
    geometry.reset();
    image.reset();

    EXPECT_EQ(packet.first_vertex, 8u);
    EXPECT_EQ(packet.vertex_count, 4u);
    EXPECT_EQ(samples, 1u);
    ASSERT_FALSE(retained_geometry.expired());
    ASSERT_FALSE(retained_image.expired());
    EXPECT_EQ(packet.geometry, retained_geometry.lock());
    EXPECT_EQ(std::get<CE::Rendering::Parameters::ImageBinding>(packet.parameters.at("image")).image, retained_image.lock());
    EXPECT_EQ(retained_geometry.lock()->native_calls, 0);
    EXPECT_EQ(retained_image.lock()->native_calls, 0);
}
