#include <assets/submission/tilemap2d.h>

#include <gtest/gtest.h>
#include <internals/exceptions.h>

#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#ifdef GL_VERSION_3_3
#error Tilemap packet resolution must not include OpenGL.
#endif

namespace {
    namespace Maps = CE::Tilemaps;
    namespace Submit = CE::Rendering::Submission;
    namespace Tiles = CE::Assets::Tilesets;
    namespace Render = CE::Rendering;
    using CE::Exceptions::bad_request;
    using CE::Exceptions::invalid_args;
    using namespace std::chrono_literals;

    struct Geometry final : Render::Geometry::Buffer2D {
        mutable int native_calls{};
        mutable int metadata_queries{};
        std::size_t count = 16;
        Render::Geometry::Topology uploaded = Render::Geometry::Topology::TriangleStrip;

        Render::Geometry::Layout2D vertex_layout() const noexcept override { return Render::Geometry::Layout2D::Position3UV2; }
        Render::Geometry::Topology topology() const noexcept override { return uploaded; }
        std::size_t vertex_count() const noexcept override {
            ++metadata_queries;
            return count;
        }
        void bind() const override { ++native_calls; }
        void draw(std::size_t, std::size_t) const override { ++native_calls; }
    };

    struct Image final : Render::Resources::Image {
        mutable int native_calls{};

        Render::Geometry::PixelSize pixel_size() const override { return {64, 32}; }
        void bind(std::uint32_t) const override { ++native_calls; }
    };

    struct Pipeline final : Render::Resources::Pipeline {
        static Render::Pipelines::Definition recipe() {
            Render::Pipelines::Definition result;
            result.program_sources = {"tilemap.vert", "tilemap.frag"};
            result.topology = Render::Geometry::Topology::TriangleStrip;
            result.parameters = {
                {"model", Render::Parameters::Type::Mat4, true, Render::Parameters::Semantic::Model},
                {"view", Render::Parameters::Type::Mat4, true, Render::Parameters::Semantic::View},
                {"projection", Render::Parameters::Type::Mat4, true, Render::Parameters::Semantic::Projection},
                {"alpha", Render::Parameters::Type::Float, true, Render::Parameters::Semantic::Alpha},
                {"scale", Render::Parameters::Type::Float, true, Render::Parameters::Semantic::Scale},
                {"image", Render::Parameters::Type::Sampler2D, true},
                {"weight", Render::Parameters::Type::Float, false}
            };
            return result;
        }

        Pipeline()
        : Render::Resources::Pipeline(recipe()) {}
    };

    struct Fixture {
        std::shared_ptr<Geometry> geometry = std::make_shared<Geometry>();
        std::shared_ptr<Image> image = std::make_shared<Image>();
        std::shared_ptr<Pipeline> pipeline = std::make_shared<Pipeline>();
        std::shared_ptr<const Render::Resources::Material> material =
            std::make_shared<Render::Resources::Material>(Render::Materials::Definition{pipeline, {}});

        static Tiles::Definition definition() {
            Tiles::Definition result;
            result.grid.frame = {16, 8};
            result.grid.rows = 1;
            result.grid.columns = 4;
            result.pivot = {0.25f, 0.75f};
            return result;
        }

        std::shared_ptr<const Tiles::Asset> asset(Tiles::Definition definition = Fixture::definition()) const {
            return std::make_shared<Tiles::Asset>(Tiles::Data{geometry, image, std::move(definition), material});
        }

        void expect_cpu() const {
            EXPECT_EQ(geometry->native_calls, 0);
            EXPECT_EQ(image->native_calls, 0);
        }
    };

    Submit::TilemapView full_view() {
        return {{100, 200}, {20, 12}, {0, 0, 60, 24}, 0ms};
    }

    glm::mat4 model(const Render::DrawPacket2D& packet) {
        return std::get<glm::mat4>(packet.parameters.at("model"));
    }

    std::vector<std::size_t> first_vertices(const std::vector<Render::DrawPacket2D>& packets) {
        std::vector<std::size_t> result;
        for (const auto& packet : packets)
            result.push_back(packet.first_vertex);
        return result;
    }
}

TEST(tilemap_submission, caller) {
    const Fixture fixture;
    const auto retained_tileset = fixture.asset();
    const Render::DrawStyle2D style{};
    const Submit::Context2D context{};
    const auto view = full_view();
    Submit::TilemapBindings bindings;
    bindings.add({7}, retained_tileset, 0);
    Maps::Map map({3, 2}, 2);
    map.set({0}, {1, 0}, {7});
    const auto packets = Submit::resolve_tilemap_layer(map, {0}, bindings, view, style, context);
    ASSERT_EQ(packets.size(), 1u);
    EXPECT_EQ(packets[0].first_vertex, 0u); // Logical 7 draws atlas zero; empty cells emitted nothing.
    EXPECT_EQ(packets[0].vertex_count, 4u);
    EXPECT_EQ(packets[0].material, fixture.material);
    fixture.expect_cpu();
}

TEST(tilemap_submission, order) {
    const Fixture fixture;
    const auto asset = fixture.asset();
    Submit::TilemapBindings bindings;
    for (std::uint32_t id = 7; id < 11; ++id)
        bindings.add({id}, asset, id - 7);
    Maps::Map map({3, 2}, 2);
    map.set({0}, {2, 0}, {10});
    map.set({0}, {0, 0}, {7});
    map.set({0}, {0, 1}, {8});
    map.set({0}, {2, 1}, {9});
    map.set({1}, {0, 0}, {10});
    const auto ground = Submit::resolve_tilemap_layer(map, {0}, bindings, full_view(), {}, {});
    const auto detail = Submit::resolve_tilemap_layer(map, {1}, bindings, full_view(), {}, {});
    EXPECT_EQ(first_vertices(ground), (std::vector<std::size_t>{0, 12, 4, 8}));
    EXPECT_EQ(first_vertices(detail), (std::vector<std::size_t>{12}));
    for (const auto& packet : ground)
        EXPECT_EQ(packet.authored_order, 0u); // Only a later pass writer assigns these.
    fixture.expect_cpu();
}

TEST(tilemap_submission, missing) {
    const Fixture fixture;
    Submit::TilemapBindings bindings;
    bindings.add({7}, fixture.asset(), 0);
    Maps::Map map({3, 2}, 2);
    map.set({1}, {0, 0}, {7});
    map.set({1}, {1, 0}, {999});
    auto view = full_view();
    view.visible = {0, 0, 20, 12};
    const Render::DrawStyle2D style{};
    const Submit::Context2D context{};
    auto retained = Submit::resolve_tilemap_layer(map, {1}, bindings, view, style, context);
    ASSERT_EQ(retained.size(), 1u); // The offscreen unbound ID was ignored.
    const auto before = fixture.geometry->metadata_queries;
    view.visible = full_view().visible;
    try {
        retained = Submit::resolve_tilemap_layer(map, {1}, bindings, view, style, context);
        FAIL() << "Expected the second visible cell to fail";
    } catch (const bad_request& error) {
        const std::string diagnostic = error.what();
        EXPECT_NE(diagnostic.find("logical ID 999"), std::string::npos);
        EXPECT_NE(diagnostic.find("layer 1"), std::string::npos);
        EXPECT_NE(diagnostic.find("cell (1,0)"), std::string::npos);
    }
    EXPECT_GT(fixture.geometry->metadata_queries, before); // First cell reached packet validation.
    EXPECT_EQ(first_vertices(retained), (std::vector<std::size_t>{0}));
    EXPECT_EQ(map.get({1}, {1, 0}), (Maps::TileId{999}));
    EXPECT_EQ(style.model_matrix, glm::mat4{1});
    EXPECT_TRUE(style.parameters.empty());
    EXPECT_TRUE(context.parameters.empty());
    fixture.expect_cpu();
}

TEST(tilemap_submission, bindings) {
    const Fixture fixture;
    const auto asset = fixture.asset();
    Submit::TilemapBindings bindings;
    bindings.add({7}, asset, 0);
    EXPECT_THROW(bindings.add({0}, asset, 1), invalid_args);
    EXPECT_THROW(bindings.add({7}, asset, 1), invalid_args);
    EXPECT_THROW(bindings.add({8}, nullptr, 0), invalid_args);
    EXPECT_THROW(bindings.add({8}, asset, 4), invalid_args);
    EXPECT_THROW(bindings.add({8}, asset, std::numeric_limits<std::size_t>::max()), invalid_args);
    Maps::Map map({3, 2}, 2);
    map.set({0}, {0, 0}, {7});
    EXPECT_EQ(first_vertices(Submit::resolve_tilemap_layer(map, {0}, bindings, full_view(), {}, {})), (std::vector<std::size_t>{0}));
    auto copy = bindings;
    copy.add({8}, asset, 1);
    map.set({0}, {0, 0}, {8});
    EXPECT_THROW(static_cast<void>(Submit::resolve_tilemap_layer(map, {0}, bindings, full_view(), {}, {})), bad_request);
    EXPECT_EQ(first_vertices(Submit::resolve_tilemap_layer(map, {0}, copy, full_view(), {}, {})), (std::vector<std::size_t>{4}));
    fixture.expect_cpu();
}

TEST(tilemap_submission, source_metadata) {
    const Fixture fixture;
    Submit::TilemapBindings bindings;
    bindings.add({7}, fixture.asset(), 0);
    const auto reject = [&](Tiles::Definition definition) {
        const auto invalid = fixture.asset(std::move(definition)); // Construct outside the negative assertion.
        EXPECT_THROW(bindings.add({8}, invalid, 0), invalid_args);
    };
    for (const auto axis : {0, 1, 2, 3}) {
        auto definition = Fixture::definition();
        if (axis == 0)
            definition.grid.frame.width = 0;
        if (axis == 1)
            definition.grid.frame.height = 0;
        if (axis == 2)
            definition.grid.columns = 0;
        if (axis == 3)
            definition.grid.rows = 0;
        reject(std::move(definition));
    }
    for (const auto value : {-0.1f, 1.1f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        for (const auto x : {true, false}) {
            auto definition = Fixture::definition();
            (x ? definition.pivot.x : definition.pivot.y) = value;
            reject(std::move(definition));
        }
    auto definition = Fixture::definition();
    definition.grid.columns = std::numeric_limits<std::size_t>::max();
    definition.grid.rows = 2;
    reject(definition);
    definition.grid.rows = 1;
    reject(definition); // Unaddressable vertex count; no allocation/OOM probe.
    for (const auto member : {&CE::Assets::Grids::Definition::origin, &CE::Assets::Grids::Definition::spacing}) {
        definition = Fixture::definition();
        definition.grid.*member = {std::numeric_limits<std::uint32_t>::max(), 0};
        reject(definition);
        definition.grid.*member = {0, std::numeric_limits<std::uint32_t>::max()};
        definition.grid.rows = 2;
        reject(definition);
    }
    definition = Fixture::definition();
    definition.grid.frame.height = 33;
    reject(definition);
    const auto no_image = std::make_shared<Tiles::Asset>(Tiles::Data{fixture.geometry, nullptr, Fixture::definition(), fixture.material});
    EXPECT_THROW(bindings.add({8}, no_image, 0), invalid_args);
    Maps::Map map({3, 2}, 2);
    map.set({0}, {0, 0}, {7});
    EXPECT_EQ(first_vertices(Submit::resolve_tilemap_layer(map, {0}, bindings, full_view(), {}, {})), (std::vector<std::size_t>{0}));
    map.set({0}, {0, 0}, {8});
    EXPECT_THROW(static_cast<void>(Submit::resolve_tilemap_layer(map, {0}, bindings, full_view(), {}, {})), bad_request);
    fixture.expect_cpu();
}

TEST(tilemap_submission, animation_metadata) {
    const Fixture fixture;
    Submit::TilemapBindings bindings;
    bindings.add({7}, fixture.asset(), 0);
    Tiles::Clip valid;
    valid.target = 0;
    valid.frames = {{1, 10ms}, {2, 20ms}};
    const auto reject = [&](Tiles::Clip clip) {
        auto definition = Fixture::definition();
        definition.animations.emplace("water", std::move(clip));
        const auto asset = fixture.asset(std::move(definition));
        EXPECT_THROW(bindings.add({8}, asset, 0), invalid_args);
    };
    auto clip = valid;
    clip.target = 4;
    reject(clip);
    clip = valid;
    clip.frames[1].cell = 4;
    reject(clip); // Check all frames, not just the one selected at setup time.
    clip.frames.clear();
    reject(clip);
    for (const auto duration : {0ms, -1ms, std::chrono::milliseconds::max()}) {
        clip = valid;
        clip.frames[0].duration = duration;
        reject(clip);
    }
    Maps::Map map({3, 2}, 2);
    map.set({0}, {0, 0}, {7});
    EXPECT_EQ(first_vertices(Submit::resolve_tilemap_layer(map, {0}, bindings, full_view(), {}, {})), (std::vector<std::size_t>{0}));
    bindings.add({8}, fixture.asset(), 1); // Failed insertions left this ID available.
    fixture.expect_cpu();
}

TEST(tilemap_submission, placement) {
    const Fixture fixture;
    Maps::Map map({3, 2}, 2);
    map.set({0}, {0, 0}, {7});
    map.set({0}, {2, 1}, {7});
    for (const Render::Geometry::Pivot pivot :
         {Render::Geometry::Pivot{0, 0}, Render::Geometry::Pivot{1, 1}, Render::Geometry::Pivot{0.25f, 0.75f}}) {
        auto definition = Fixture::definition();
        definition.pivot = pivot;
        Submit::TilemapBindings bindings;
        bindings.add({7}, fixture.asset(definition), 0);
        const auto packets = Submit::resolve_tilemap_layer(map, {0}, bindings, full_view(), {}, {});
        ASSERT_EQ(packets.size(), 2u);
        const auto strip = Render::Geometry::Anchor::MakeQuadStrip(pivot, 64, 32, 16, 8);
        for (std::size_t i = 0; i < packets.size(); ++i) {
            const auto transform = model(packets[i]);
            EXPECT_FLOAT_EQ(transform[0][0], 1.25f);
            EXPECT_FLOAT_EQ(transform[1][1], 1.5f);
            const auto left = i == 0 ? 100.0f : 140.0f;
            const auto top = i == 0 ? 200.0f : 188.0f;
            const std::array expected{
                glm::vec2{left, top - 12}, glm::vec2{left + 20, top - 12}, glm::vec2{left, top}, glm::vec2{left + 20, top}
            };
            const auto vertices = strip.vertices();
            for (std::size_t corner = 0; corner < vertices.size(); ++corner) {
                const auto& vertex = vertices[corner];
                const auto placed = transform * glm::vec4{vertex.x, vertex.y, vertex.z, 1};
                EXPECT_FLOAT_EQ(placed.x, expected[corner].x);
                EXPECT_FLOAT_EQ(placed.y, expected[corner].y);
            }
        }
    }
    fixture.expect_cpu();
}

TEST(tilemap_submission, style_context) {
    const Fixture fixture;
    Submit::TilemapBindings bindings;
    bindings.add({7}, fixture.asset(), 0);
    Maps::Map map({3, 2}, 2);
    map.set({0}, {0, 0}, {7});
    Render::DrawStyle2D style;
    style.material = std::make_shared<Render::Resources::Material>(Render::Materials::Definition{std::make_shared<Pipeline>(), {}});
    style.alpha = 0.4f;
    style.parameters.emplace("weight", 0.75f);
    style.clip = Render::ClipRegion2D{{1, 2, 10, 20}, 100, 200};
    Submit::Context2D context;
    context.image = Submit::ImageParameter2D{"image", 2};
    context.parameters.emplace("weight", 0.25f);
    context.pass.projection[3][0] = 19;
    context.pass.view[3][1] = -3;
    const auto packets = Submit::resolve_tilemap_layer(map, {0}, bindings, full_view(), style, context);
    ASSERT_EQ(packets.size(), 1u);
    const auto& packet = packets[0];
    EXPECT_EQ(packet.material, style.material);
    EXPECT_NE(packet.material, fixture.material);
    EXPECT_FLOAT_EQ(std::get<float>(packet.parameters.at("alpha")), 0.4f);
    EXPECT_FLOAT_EQ(std::get<float>(packet.parameters.at("scale")), 1.0f);
    EXPECT_FLOAT_EQ(std::get<float>(packet.parameters.at("weight")), 0.75f);
    EXPECT_EQ(packet.clip, style.clip);
    EXPECT_EQ(std::get<Render::Parameters::ImageBinding>(packet.parameters.at("image")).unit, 2u);
    EXPECT_EQ(style.model_matrix, glm::mat4{1});
    EXPECT_FALSE(style.parameters.contains("image"));
    context.pass.projection[3][0] = 90;
    context.pass.view[3][1] = 80;
    style.clip.reset();
    style.parameters.at("weight") = 5.0f;
    EXPECT_FLOAT_EQ(std::get<glm::mat4>(packet.parameters.at("projection"))[3][0], 19);
    EXPECT_FLOAT_EQ(std::get<glm::mat4>(packet.parameters.at("view"))[3][1], -3);
    EXPECT_FLOAT_EQ(std::get<float>(packet.parameters.at("weight")), 0.75f);
    EXPECT_TRUE(packet.clip.has_value());
    fixture.expect_cpu();
}

TEST(tilemap_submission, animation) {
    const Fixture fixture;
    auto definition = Fixture::definition();
    Tiles::Clip first;
    first.target = 0;
    first.frames = {{1, 10ms}, {2, 20ms}};
    first.loop = true;
    definition.animations.emplace("first", first);
    Tiles::Clip second;
    second.target = 1;
    second.frames = {{3, 5ms}}; // If the adapter animates twice, target 0 incorrectly becomes 3.
    definition.animations.emplace("second", second);
    Tiles::Clip held;
    held.target = 3;
    held.frames = {{0, 10ms}, {2, 20ms}};
    definition.animations.emplace("held", held);
    const auto asset = fixture.asset(std::move(definition));
    Submit::TilemapBindings bindings;
    bindings.add({7}, asset, 0);
    bindings.add({8}, asset, 3);
    Maps::Map west({3, 2}, 2), east({3, 2}, 2);
    west.set({0}, {0, 0}, {7});
    east.set({0}, {0, 0}, {7});
    auto west_view = full_view();
    auto east_view = full_view();
    east_view.elapsed = 10ms;
    const auto old = Submit::resolve_tilemap_layer(west, {0}, bindings, west_view, {}, {});
    EXPECT_EQ(first_vertices(old), (std::vector<std::size_t>{4}));
    EXPECT_EQ(first_vertices(Submit::resolve_tilemap_layer(east, {0}, bindings, east_view, {}, {})), (std::vector<std::size_t>{8}));
    west_view.elapsed = 30ms;
    EXPECT_EQ(first_vertices(Submit::resolve_tilemap_layer(west, {0}, bindings, west_view, {}, {})), (std::vector<std::size_t>{4}));
    west.set({0}, {0, 0}, {});
    EXPECT_TRUE(Submit::resolve_tilemap_layer(west, {0}, bindings, west_view, {}, {}).empty());
    EXPECT_EQ(first_vertices(Submit::resolve_tilemap_layer(east, {0}, bindings, east_view, {}, {})), (std::vector<std::size_t>{8}));
    west.set({0}, {0, 0}, {8});
    west_view.elapsed = std::chrono::milliseconds::max();
    EXPECT_EQ(first_vertices(Submit::resolve_tilemap_layer(west, {0}, bindings, west_view, {}, {})), (std::vector<std::size_t>{8}));
    EXPECT_EQ(first_vertices(old), (std::vector<std::size_t>{4}));
    EXPECT_EQ(asset->cell_at(0, 0ms), 1u); // Shared asset time/targets remain unchanged.
    fixture.expect_cpu();
}

TEST(tilemap_submission, retention) {
    std::vector<Render::DrawPacket2D> packets;
    std::weak_ptr<const Tiles::Asset> asset_lifetime;
    std::weak_ptr<Geometry> geometry;
    std::weak_ptr<Image> image;
    std::weak_ptr<const Render::Resources::Material> material;
    std::weak_ptr<Pipeline> pipeline;
    {
        const Fixture fixture;
        geometry = fixture.geometry;
        image = fixture.image;
        material = fixture.material;
        pipeline = fixture.pipeline;
        auto asset = fixture.asset();
        asset_lifetime = asset;
        Submit::TilemapBindings bindings;
        bindings.add({7}, asset, 0);
        asset.reset();
        ASSERT_FALSE(asset_lifetime.expired());
        Maps::Map map({3, 2}, 2);
        map.set({0}, {0, 0}, {7});
        auto view = full_view();
        packets = Submit::resolve_tilemap_layer(map, {0}, bindings, view, {}, {});
        view.origin = {900, 900};
        map.set({0}, {0, 0}, {});
        EXPECT_EQ(first_vertices(packets), (std::vector<std::size_t>{0}));
        fixture.expect_cpu();
    }
    EXPECT_TRUE(asset_lifetime.expired());
    ASSERT_FALSE(geometry.expired());
    ASSERT_FALSE(image.expired());
    EXPECT_FALSE(material.expired());
    EXPECT_FALSE(pipeline.expired());
    EXPECT_FLOAT_EQ(model(packets[0])[3][0], 105);
    EXPECT_FLOAT_EQ(model(packets[0])[3][1], 191);
    EXPECT_EQ(geometry.lock()->native_calls, 0);
    EXPECT_EQ(image.lock()->native_calls, 0);
    packets.clear();
    EXPECT_TRUE(geometry.expired());
    EXPECT_TRUE(image.expired());
    EXPECT_TRUE(material.expired());
    EXPECT_TRUE(pipeline.expired());
}

TEST(tilemap_submission, invalid_inputs) {
    const Fixture fixture;
    Submit::TilemapBindings bindings;
    bindings.add({7}, fixture.asset(), 0);
    Maps::Map map({3, 2}, 2); // Empty cells do not hide malformed layer/view/transform inputs.
    for (const Maps::LayerIndex layer :
         {Maps::LayerIndex{-1}, Maps::LayerIndex{2}, Maps::LayerIndex{std::numeric_limits<std::int64_t>::max()}})
        EXPECT_THROW(static_cast<void>(Submit::resolve_tilemap_layer(map, layer, bindings, full_view(), {}, {})), bad_request);
    auto view = full_view();
    view.elapsed = -1ms;
    EXPECT_THROW(static_cast<void>(Submit::resolve_tilemap_layer(map, {0}, bindings, view, {}, {})), invalid_args);
    view = full_view();
    view.visible = {0, 0, 0, 0};
    for (const auto value :
         {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::max()}) {
        view.origin = {value, 200};
        EXPECT_THROW(static_cast<void>(Submit::resolve_tilemap_layer(map, {0}, bindings, view, {}, {})), invalid_args);
        view.origin = {100, value};
        EXPECT_THROW(static_cast<void>(Submit::resolve_tilemap_layer(map, {0}, bindings, view, {}, {})), invalid_args);
    }
    for (const auto scale : {0.0f, -1.0f, 2.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
        Render::DrawStyle2D style;
        style.scale = scale;
        EXPECT_THROW(static_cast<void>(Submit::resolve_tilemap_layer(map, {0}, bindings, full_view(), style, {})), invalid_args);
    }
    for (const auto value : {0.5f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        Render::DrawStyle2D style;
        style.model_matrix[3][0] = value;
        EXPECT_THROW(static_cast<void>(Submit::resolve_tilemap_layer(map, {0}, bindings, full_view(), style, {})), invalid_args);
        Submit::Context2D context;
        if (!std::isfinite(value)) {
            context.pass.projection[0][0] = value;
            EXPECT_THROW(static_cast<void>(Submit::resolve_tilemap_layer(map, {0}, bindings, full_view(), {}, context)), invalid_args);
            context.pass.projection = glm::mat4{1};
            context.pass.view[1][1] = value;
            EXPECT_THROW(static_cast<void>(Submit::resolve_tilemap_layer(map, {0}, bindings, full_view(), {}, context)), invalid_args);
        }
    }
    Render::DrawStyle2D rotated;
    rotated.model_matrix[0][0] = 0;
    rotated.model_matrix[0][1] = 1;
    EXPECT_THROW(static_cast<void>(Submit::resolve_tilemap_layer(map, {0}, bindings, full_view(), rotated, {})), invalid_args);
    Render::DrawStyle2D clipped;
    clipped.clip = Render::ClipRegion2D{{2, 0, 1, 1}, 100, 100};
    EXPECT_THROW(static_cast<void>(Submit::resolve_tilemap_layer(map, {0}, bindings, full_view(), clipped, {})), invalid_args);
    Maps::Map moved(std::move(map));
    EXPECT_THROW(static_cast<void>(Submit::resolve_tilemap_layer(map, {0}, bindings, full_view(), {}, {})), bad_request);
    EXPECT_TRUE(Submit::resolve_tilemap_layer(moved, {0}, bindings, full_view(), {}, {}).empty());
    fixture.expect_cpu();
}

TEST(tilemap_submission, float_limits) {
    const Fixture fixture;
    Submit::TilemapBindings bindings;
    bindings.add({7}, fixture.asset(), 0);
    Maps::Map map({3, 2}, 2);
    map.set({0}, {0, 0}, {7});
    auto view = full_view();
    const auto maximum = static_cast<double>(std::numeric_limits<float>::max());
    view.origin.x = maximum;
    view.cell_size.width = maximum / 4;
    EXPECT_THROW(static_cast<void>(Submit::resolve_tilemap_layer(map, {0}, bindings, view, {}, {})), invalid_args);
    view = full_view();
    view.origin.y = -maximum;
    view.cell_size.height = maximum / 4;
    EXPECT_THROW(static_cast<void>(Submit::resolve_tilemap_layer(map, {0}, bindings, view, {}, {})), invalid_args);
    for (const auto tiny : {std::numeric_limits<double>::denorm_min(), static_cast<double>(std::numeric_limits<float>::denorm_min())}) {
        view = {{0, 0}, {tiny, tiny}, {0, 0, 3 * tiny, 2 * tiny}, 0ms};
        EXPECT_THROW(static_cast<void>(Submit::resolve_tilemap_layer(map, {0}, bindings, view, {}, {})), invalid_args);
    }
    fixture.expect_cpu();
}

TEST(tilemap_submission, resource_failure) {
    const Fixture fixture;
    Fixture invalid;
    invalid.geometry->count = 0; // Valid metadata; resource range validation belongs to resolve_tile().
    Submit::TilemapBindings bindings;
    bindings.add({7}, fixture.asset(), 0);
    bindings.add({8}, invalid.asset(), 0);
    Maps::Map map({3, 2}, 2);
    map.set({0}, {0, 0}, {7});
    auto previous = Submit::resolve_tilemap_layer(map, {0}, bindings, full_view(), {}, {});
    const auto before = fixture.geometry->metadata_queries;
    map.set({0}, {1, 0}, {8});
    EXPECT_THROW(previous = Submit::resolve_tilemap_layer(map, {0}, bindings, full_view(), {}, {}), invalid_args);
    EXPECT_GT(fixture.geometry->metadata_queries, before);
    EXPECT_EQ(first_vertices(previous), (std::vector<std::size_t>{0}));
    EXPECT_EQ(map.get({0}, {1, 0}), (Maps::TileId{8}));
    auto view = full_view();
    view.visible = {0, 0, 20, 12};
    EXPECT_EQ(first_vertices(Submit::resolve_tilemap_layer(map, {0}, bindings, view, {}, {})), (std::vector<std::size_t>{0}));
    fixture.expect_cpu();
    invalid.expect_cpu();
}

TEST(tilemap_submission, parameter_failure) {
    const Fixture fixture;
    Submit::TilemapBindings bindings;
    bindings.add({7}, fixture.asset(), 0);
    Maps::Map map({3, 2}, 2);
    map.set({0}, {0, 0}, {7});
    Render::DrawStyle2D style;
    style.parameters.emplace("image", Render::Parameters::ImageBinding{fixture.image, 0});
    EXPECT_THROW(static_cast<void>(Submit::resolve_tilemap_layer(map, {0}, bindings, full_view(), style, {})), invalid_args);
    Submit::Context2D context;
    context.image.reset(); // Existing manual image insertion remains supported.
    EXPECT_EQ(
        first_vertices(Submit::resolve_tilemap_layer(map, {0}, bindings, full_view(), style, context)), (std::vector<std::size_t>{0})
    );
    context.constraints.depth = Render::Pipelines::Depth::Less;
    EXPECT_THROW(static_cast<void>(Submit::resolve_tilemap_layer(map, {0}, bindings, full_view(), style, context)), invalid_args);
    EXPECT_EQ(style.parameters.size(), 1u);
    fixture.expect_cpu();
}

TEST(tilemap_submission, computed_visibility) {
    const Fixture fixture;
    const auto asset = fixture.asset();
    Submit::TilemapBindings bindings;
    bindings.add({7}, asset, 0);
    bindings.add({8}, asset, 1);
    Maps::Map map({64, 2}, 2);
    map.set({0}, {42, 0}, {7});
    map.set({0}, {43, 0}, {8});
    map.set({0}, {44, 0}, {999});
    const Submit::TilemapView view{{0, 0}, {0.1, 0.1}, {43 * 0.1, 0, 44 * 0.1, 0.1}, 0ms};
    EXPECT_EQ(first_vertices(Submit::resolve_tilemap_layer(map, {0}, bindings, view, {}, {})), (std::vector<std::size_t>{4}));
    auto expanded = view;
    expanded.visible.left = std::nextafter(view.visible.left, -std::numeric_limits<double>::infinity());
    EXPECT_EQ(first_vertices(Submit::resolve_tilemap_layer(map, {0}, bindings, expanded, {}, {})), (std::vector<std::size_t>{0, 4}));
    expanded.visible.right = std::nextafter(view.visible.right, std::numeric_limits<double>::infinity());
    EXPECT_THROW(static_cast<void>(Submit::resolve_tilemap_layer(map, {0}, bindings, expanded, {}, {})), bad_request);
    auto outside = view;
    outside.visible = {64 * 0.1, 0, 65 * 0.1, 0.1};
    EXPECT_TRUE(Submit::resolve_tilemap_layer(map, {0}, bindings, outside, {}, {}).empty());
    fixture.expect_cpu();
}
