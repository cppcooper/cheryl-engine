#include "tilemap-editor.h"
#include "tilemap-terrain.h"

#include <assets/submission/tilemap2d.h>
#include <core/rendering/render-frame.h>

#include <assets/resources/resource-provider.h>
#include <gtest/gtest.h>
#include <internals/exceptions.h>

#include <ext/matrix_clip_space.hpp>

#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#ifdef GL_VERSION_3_3
#error The synthetic tilemap consumer must not include OpenGL.
#endif

namespace {
    namespace Maps = CE::Tilemaps;
    namespace Submit = CE::Rendering::Submission;
    namespace Tiles = CE::Assets::Tilesets;
    namespace Render = CE::Rendering;
    using CE::Exceptions::bad_request;
    using CE::Exceptions::invalid_args;
    using namespace std::chrono_literals;
    constexpr Maps::LayerIndex ground{0}, detail{1};

    struct Calls {
        int binds{};
        int draws{};
        int uploads{};
        int geometry_queries{};

        void expect_cpu() const {
            EXPECT_EQ(binds, 0);
            EXPECT_EQ(draws, 0);
            EXPECT_EQ(uploads, 0);
        }
    };

    struct Geometry final : Render::Geometry::Buffer2D {
        std::shared_ptr<Calls> calls;

        explicit Geometry(std::shared_ptr<Calls> calls)
        : calls(std::move(calls)) {}
        Render::Geometry::Layout2D vertex_layout() const noexcept override { return Render::Geometry::Layout2D::Position3UV2; }
        Render::Geometry::Topology topology() const noexcept override { return Render::Geometry::Topology::TriangleStrip; }
        std::size_t vertex_count() const noexcept override {
            ++calls->geometry_queries;
            return 16;
        }
        void bind() const override { ++calls->binds; }
        void draw(std::size_t, std::size_t) const override { ++calls->draws; }
    };

    struct Image final : Render::Resources::Image {
        std::shared_ptr<Calls> calls;

        explicit Image(std::shared_ptr<Calls> calls)
        : calls(std::move(calls)) {}
        Render::Geometry::PixelSize pixel_size() const override { return {64, 32}; }
        void bind(std::uint32_t) const override { ++calls->binds; }
    };

    // The consumer receives retained resources, never this upload capability.
    // Stand-ins are constructed directly, so even fixture setup needs no upload.
    struct Provider final : Render::Resources::Provider {
        std::shared_ptr<Calls> calls;

        explicit Provider(std::shared_ptr<Calls> calls)
        : calls(std::move(calls)) {}
        std::shared_ptr<Render::Resources::Image> create_image(const CE::Assets::Images::Decoded&) override {
            ++calls->uploads;
            return {};
        }
        std::shared_ptr<Render::Resources::Image> create_font_atlas(std::span<const unsigned char>, Render::Geometry::PixelSize) override {
            ++calls->uploads;
            return {};
        }
        std::shared_ptr<Render::Geometry::Buffer2D>
        upload_geometry(std::span<const Render::Geometry::Vertex2D>, Render::Geometry::Topology) override {
            ++calls->uploads;
            return {};
        }
        std::shared_ptr<Render::Resources::Program> link_program(const std::vector<std::filesystem::path>&) override {
            ++calls->uploads;
            return {};
        }
    };

    struct Pipeline final : Render::Resources::Pipeline {
        static Render::Pipelines::Definition recipe() {
            Render::Pipelines::Definition definition;
            definition.program_sources = {"consumer.vert", "consumer.frag"};
            definition.topology = Render::Geometry::Topology::TriangleStrip;
            definition.parameters = {
                {"model", Render::Parameters::Type::Mat4, true, Render::Parameters::Semantic::Model},
                {"projection", Render::Parameters::Type::Mat4, true, Render::Parameters::Semantic::Projection},
                {"view", Render::Parameters::Type::Mat4, true, Render::Parameters::Semantic::View},
                {"alpha", Render::Parameters::Type::Float, true, Render::Parameters::Semantic::Alpha},
                {"scale", Render::Parameters::Type::Float, true, Render::Parameters::Semantic::Scale},
                {"image", Render::Parameters::Type::Sampler2D, true},
                {"weight", Render::Parameters::Type::Float, false, Render::Parameters::Semantic::Custom, 0.5f}
            };
            return definition;
        }

        Pipeline()
        : Render::Resources::Pipeline(recipe()) {}
    };

    struct Fixture {
        std::shared_ptr<Calls> calls;
        Provider provider;
        std::shared_ptr<Geometry> geometry;
        std::shared_ptr<Image> image;
        std::shared_ptr<Pipeline> pipeline = std::make_shared<Pipeline>();
        std::shared_ptr<const Render::Resources::Material> material =
            std::make_shared<Render::Resources::Material>(Render::Materials::Definition{pipeline, {}});
        std::shared_ptr<const Tiles::Asset> asset;
        Submit::TilemapBindings bindings;
        Maps::Map west{{3, 2}, 2}, east{{3, 2}, 2};
        Submit::TilemapView west_view{{100, 200}, {20, 12}, {0, 0, 60, 24}, 0ms};
        Submit::TilemapView east_view{{300, 200}, {20, 12}, {0, 0, 60, 24}, 0ms};

        explicit Fixture(std::shared_ptr<Calls> calls = std::make_shared<Calls>())
        : calls(std::move(calls)),
          provider(this->calls),
          geometry(std::make_shared<Geometry>(this->calls)),
          image(std::make_shared<Image>(this->calls)) {
            Tiles::Definition definition;
            definition.grid.frame = {16, 8};
            definition.grid.rows = 1;
            definition.grid.columns = 4;
            definition.pivot = {0.25f, 0.75f};
            // Actually const metadata, not merely a const view of a mutable alias.
            asset = std::make_shared<const Tiles::Asset>(Tiles::Data{geometry, image, std::move(definition), material});
            for (std::uint32_t id = 7; id <= 10; ++id)
                bindings.add({id}, asset, id - 7);
            // Deliberately author cells in a different order from row/column traversal.
            west.set(ground, {1, 1}, {9});
            west.set(ground, {2, 0}, {8});
            west.set(ground, {0, 0}, {7});
            west.set(detail, {0, 1}, {8});
            west.set(detail, {1, 0}, {10});
            east.set(ground, {2, 1}, {7});
            east.set(ground, {0, 0}, {9});
            east.set(detail, {1, 1}, {10});
        }
    };

    struct ExpectedDraw {
        std::size_t first;
        float x, y;
    };

    constexpr std::array authored{ExpectedDraw{0, 105, 191},  ExpectedDraw{4, 145, 191}, ExpectedDraw{8, 125, 179},
                                  ExpectedDraw{12, 125, 191}, ExpectedDraw{4, 105, 179}, ExpectedDraw{8, 305, 191},
                                  ExpectedDraw{0, 345, 179},  ExpectedDraw{12, 325, 179}};

    void expect_draws(const Render::Frame& frame, const std::span<const ExpectedDraw> expected) {
        const auto passes = frame.passes(); // Borrowed only for this read; no writer/recycle is active.
        ASSERT_EQ(passes.size(), 1u);
        const auto& draws = passes[0].draws;
        ASSERT_EQ(draws.size(), expected.size());
        for (std::size_t i = 0; i < draws.size(); ++i) {
            EXPECT_EQ(draws[i].authored_order, i);
            EXPECT_EQ(draws[i].first_vertex, expected[i].first);
            EXPECT_EQ(draws[i].vertex_count, 4u);
            const auto model = std::get<glm::mat4>(draws[i].parameters.at("model"));
            EXPECT_FLOAT_EQ(model[3][0], expected[i].x);
            EXPECT_FLOAT_EQ(model[3][1], expected[i].y);
            EXPECT_FLOAT_EQ(model[3][2], 0); // No hidden per-layer Z policy.
        }
    }

    void expect_payload(const Render::DrawPacket2D& actual, const Render::DrawPacket2D& expected) {
        EXPECT_EQ(actual.geometry, expected.geometry);
        EXPECT_EQ(actual.material, expected.material);
        EXPECT_EQ(actual.first_vertex, expected.first_vertex);
        EXPECT_EQ(actual.vertex_count, expected.vertex_count);
        EXPECT_EQ(actual.order_sensitive, expected.order_sensitive);
        EXPECT_EQ(actual.clip, expected.clip);
        EXPECT_EQ(actual.parameters.size(), expected.parameters.size());
        for (const auto* key : {"model", "projection", "view"})
            EXPECT_EQ(std::get<glm::mat4>(actual.parameters.at(key)), std::get<glm::mat4>(expected.parameters.at(key)));
        for (const auto* key : {"alpha", "scale", "weight"})
            EXPECT_FLOAT_EQ(std::get<float>(actual.parameters.at(key)), std::get<float>(expected.parameters.at(key)));
        const auto& image = std::get<Render::Parameters::ImageBinding>(actual.parameters.at("image"));
        const auto& old_image = std::get<Render::Parameters::ImageBinding>(expected.parameters.at("image"));
        EXPECT_EQ(image.image, old_image.image);
        EXPECT_EQ(image.sampler, old_image.sampler);
        EXPECT_EQ(image.unit, old_image.unit);
    }

    void expect_packets(const Render::Frame& frame, const std::vector<Render::DrawPacket2D>& expected) {
        const auto passes = frame.passes();
        ASSERT_EQ(passes.size(), 1u);
        ASSERT_EQ(passes[0].draws.size(), expected.size());
        for (std::size_t i = 0; i < expected.size(); ++i) {
            EXPECT_EQ(passes[0].draws[i].authored_order, i);
            expect_payload(passes[0].draws[i], expected[i]);
        }
    }

    std::vector<Maps::TileId> cells(const Maps::Map& map) {
        std::vector<Maps::TileId> result;
        for (std::uint32_t layer = 0; layer < map.layer_count(); ++layer)
            for (std::uint32_t row = 0; row < map.extent().rows; ++row)
                for (std::uint32_t column = 0; column < map.extent().columns; ++column)
                    result.push_back(map.get({layer}, {column, row}));
        return result;
    }

    // Consumer-local camera-plane values, X-right/Y-up. No input/window/camera
    // owner is retained: view = (world - pan) * zoom, with a [0,w] x [0,h] viewport.
    struct ViewSample {
        Maps::LocalPoint pan;
        double zoom;
        Maps::CellSize viewport;
    };

    float view_float(const double value, const bool positive = false) {
        if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max() || (positive && value <= 0))
            throw invalid_args(CE_HERE, "View values must be finite and float-representable, with positive zoom/viewport");
        const auto result = static_cast<float>(value);
        if (positive && result <= 0)
            throw invalid_args(CE_HERE, "View scale/dimensions must not collapse to zero");
        return result;
    }

    Render::Parameters::PassSemantics view_matrices(const ViewSample& view) {
        const auto zoom = view_float(view.zoom, true);
        const auto width = view_float(view.viewport.width, true);
        const auto height = view_float(view.viewport.height, true);
        Render::Parameters::PassSemantics result;
        result.projection = glm::ortho(0.0f, width, 0.0f, height);
        result.view = glm::mat4{1};
        result.view[0][0] = result.view[1][1] = zoom;
        result.view[3][0] = view_float(-view.pan.x * view.zoom);
        result.view[3][1] = view_float(-view.pan.y * view.zoom);
        for (const auto& matrix : {result.projection, result.view})
            for (int column = 0; column < 4; ++column)
                for (int row = 0; row < 4; ++row)
                    if (!std::isfinite(matrix[column][row]))
                        throw invalid_args(CE_HERE, "View matrices must remain finite");
        return result;
    }

    Maps::LocalPoint to_local(const ViewSample& view, const glm::dvec2 origin, const Maps::LocalPoint point) {
        static_cast<void>(view_matrices(view)); // Reject invalid transform values before any edit.
        if (!std::isfinite(origin.x) || !std::isfinite(origin.y) || !std::isfinite(point.x) || !std::isfinite(point.y))
            throw invalid_args(CE_HERE, "Map origin and picked view point must be finite");
        const Maps::LocalPoint local{view.pan.x + point.x / view.zoom - origin.x, origin.y - (view.pan.y + point.y / view.zoom)};
        if (!std::isfinite(local.x) || !std::isfinite(local.y))
            throw invalid_args(CE_HERE, "Point conversion is not finite");
        return local;
    }

    Maps::LocalRect local_visible(const ViewSample& camera, const glm::dvec2 origin) {
        const auto top_left = to_local(camera, origin, {0, camera.viewport.height});
        const auto bottom_right = to_local(camera, origin, {camera.viewport.width, 0});
        if (top_left.x >= bottom_right.x || top_left.y >= bottom_right.y)
            throw invalid_args(CE_HERE, "Transformed viewport must have positive area");
        return {top_left.x, top_left.y, bottom_right.x, bottom_right.y};
    }

    std::optional<Maps::Point> paint_at(
        Maps::Map& map,
        const Maps::LayerIndex layer,
        const Submit::TilemapView& placement,
        const ViewSample& camera,
        const Maps::LocalPoint point,
        const Maps::TileId tile
    ) {
        static_cast<void>(local_visible(camera, placement.origin));
        const auto cell = Maps::pick_cell(map.extent(), placement.cell_size, to_local(camera, placement.origin, point));
        if (cell)
            map.set(layer, *cell, tile);
        return cell;
    }

    void prepare_view(Render::Frame& frame, const Fixture& fixture, const ViewSample& camera) {
        auto west = fixture.west_view;
        auto east = fixture.east_view;
        west.visible = local_visible(camera, west.origin);
        east.visible = local_visible(camera, east.origin);
        const auto matrices = view_matrices(camera);
        Render::FrameWriter writer(frame);
        auto pass = writer.begin_pass(matrices.projection, matrices.view);
        const Submit::Context2D context{pass.semantics(), pass.parameters(), pass.constraints()};
        pass.add(Submit::resolve_tilemap_layer(fixture.west, ground, fixture.bindings, west, {}, context));
        pass.add(Submit::resolve_tilemap_layer(fixture.west, detail, fixture.bindings, west, {}, context));
        pass.add(Submit::resolve_tilemap_layer(fixture.east, ground, fixture.bindings, east, {}, context));
        pass.add(Submit::resolve_tilemap_layer(fixture.east, detail, fixture.bindings, east, {}, context));
    }
}

TEST(tilemap_consumer, caller) {
    const Fixture fixture;
    const Render::DrawStyle2D style{};
    auto projection = glm::mat4{2};
    auto camera_view = glm::mat4{1};
    camera_view[3][0] = -17;
    Render::Parameters::Set parameters{{"weight", 0.375f}};
    Render::Pipelines::PassConstraints2D constraints;
    constraints.depth_write = false;
    Render::Frame frame;
    {
        Render::FrameWriter writer(frame);
        auto pass = writer.begin_pass(projection, camera_view, constraints, parameters);
        const Submit::Context2D context{pass.semantics(), pass.parameters(), pass.constraints()};
        pass.add(Submit::resolve_tilemap_layer(fixture.west, ground, fixture.bindings, fixture.west_view, style, context));
        pass.add(Submit::resolve_tilemap_layer(fixture.west, detail, fixture.bindings, fixture.west_view, style, context));
        pass.add(Submit::resolve_tilemap_layer(fixture.east, ground, fixture.bindings, fixture.east_view, style, context));
        pass.add(Submit::resolve_tilemap_layer(fixture.east, detail, fixture.bindings, fixture.east_view, style, context));
    }
    expect_draws(frame, authored);
    projection[0][0] = 90;
    camera_view[3][0] = -99;
    parameters.at("weight") = 0.9f;
    constraints.depth_write = true;
    const auto passes = frame.passes();
    ASSERT_EQ(passes.size(), 1u);
    EXPECT_FLOAT_EQ(passes[0].projection[0][0], 2);
    EXPECT_FLOAT_EQ(passes[0].view[3][0], -17);
    EXPECT_EQ(passes[0].constraints.depth_write, false);
    for (const auto& packet : passes[0].draws) {
        EXPECT_FLOAT_EQ(std::get<glm::mat4>(packet.parameters.at("projection"))[0][0], 2);
        EXPECT_FLOAT_EQ(std::get<glm::mat4>(packet.parameters.at("view"))[3][0], -17);
        EXPECT_FLOAT_EQ(std::get<float>(packet.parameters.at("weight")), 0.375f);
        EXPECT_EQ(packet.geometry, fixture.geometry);
        EXPECT_EQ(packet.material, fixture.material);
        EXPECT_EQ(std::get<Render::Parameters::ImageBinding>(packet.parameters.at("image")).image, fixture.image);
    }
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, sequence) {
    Fixture fixture;
    const auto other_image = std::make_shared<Image>(fixture.calls);
    const auto other_material = std::make_shared<Render::Resources::Material>(Render::Materials::Definition{fixture.pipeline, {}});
    const auto other_asset =
        std::make_shared<const Tiles::Asset>(Tiles::Data{fixture.geometry, other_image, fixture.asset->definition(), other_material});
    fixture.bindings.add({11}, other_asset, 0);
    fixture.west.set(ground, {0, 0}, {11}); // Same target/placement, distinct retained image/material.
    Render::Frame first, reordered;
    {
        Render::FrameWriter writer(first);
        auto pass = writer.begin_pass(glm::mat4{1}, glm::mat4{1});
        const Submit::Context2D context{pass.semantics(), pass.parameters(), pass.constraints()};
        pass.add(Submit::resolve_tilemap_layer(fixture.west, ground, fixture.bindings, fixture.west_view, {}, context));
        pass.add(Submit::resolve_tilemap_layer(fixture.west, detail, fixture.bindings, fixture.west_view, {}, context));
        pass.add(Submit::resolve_tilemap_layer(fixture.east, ground, fixture.bindings, fixture.east_view, {}, context));
        pass.add(Submit::resolve_tilemap_layer(fixture.east, detail, fixture.bindings, fixture.east_view, {}, context));
    }
    {
        Render::FrameWriter writer(reordered);
        auto pass = writer.begin_pass(glm::mat4{1}, glm::mat4{1});
        const Submit::Context2D context{pass.semantics(), pass.parameters(), pass.constraints()};
        pass.add(Submit::resolve_tilemap_layer(fixture.east, detail, fixture.bindings, fixture.east_view, {}, context));
        pass.add(Submit::resolve_tilemap_layer(fixture.east, ground, fixture.bindings, fixture.east_view, {}, context));
        pass.add(Submit::resolve_tilemap_layer(fixture.west, detail, fixture.bindings, fixture.west_view, {}, context));
        pass.add(Submit::resolve_tilemap_layer(fixture.west, ground, fixture.bindings, fixture.west_view, {}, context));
    }
    ASSERT_NO_FATAL_FAILURE(expect_draws(first, authored));
    const std::array reverse_groups{authored[7], authored[5], authored[6], authored[3], authored[4], authored[0], authored[1], authored[2]};
    ASSERT_NO_FATAL_FAILURE(expect_draws(reordered, reverse_groups));
    const auto first_passes = first.passes();
    const auto reordered_passes = reordered.passes();
    for (std::size_t i = 0; i < authored.size(); ++i) {
        EXPECT_EQ(first_passes[0].draws[i].material, i == 0 ? other_material : fixture.material);
        EXPECT_EQ(reordered_passes[0].draws[i].material, i == 5 ? other_material : fixture.material);
        EXPECT_EQ(
            std::get<Render::Parameters::ImageBinding>(first_passes[0].draws[i].parameters.at("image")).image,
            i == 0 ? other_image : fixture.image
        );
        EXPECT_EQ(
            std::get<Render::Parameters::ImageBinding>(reordered_passes[0].draws[i].parameters.at("image")).image,
            i == 5 ? other_image : fixture.image
        );
    }
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, snapshots) {
    Fixture fixture;
    Render::Frame older, newer;
    {
        Render::FrameWriter writer(older);
        auto pass = writer.begin_pass(glm::mat4{1}, glm::mat4{1});
        const Submit::Context2D context{pass.semantics(), pass.parameters(), pass.constraints()};
        pass.add(Submit::resolve_tilemap_layer(fixture.west, ground, fixture.bindings, fixture.west_view, {}, context));
        pass.add(Submit::resolve_tilemap_layer(fixture.west, detail, fixture.bindings, fixture.west_view, {}, context));
        pass.add(Submit::resolve_tilemap_layer(fixture.east, ground, fixture.bindings, fixture.east_view, {}, context));
        pass.add(Submit::resolve_tilemap_layer(fixture.east, detail, fixture.bindings, fixture.east_view, {}, context));
    }
    ASSERT_EQ(older.passes().size(), 1u);
    const auto old_packets = older.passes()[0].draws; // Owned copy, not a view across preparation/recycling.
    const auto east_cells = cells(fixture.east);
    fixture.west.set(ground, {0, 0}, {10});
    fixture.west_view.origin.x += 20;
    {
        Render::FrameWriter writer(newer);
        auto pass = writer.begin_pass(glm::mat4{1}, glm::mat4{1});
        const Submit::Context2D context{pass.semantics(), pass.parameters(), pass.constraints()};
        pass.add(Submit::resolve_tilemap_layer(fixture.west, ground, fixture.bindings, fixture.west_view, {}, context));
        pass.add(Submit::resolve_tilemap_layer(fixture.west, detail, fixture.bindings, fixture.west_view, {}, context));
        pass.add(Submit::resolve_tilemap_layer(fixture.east, ground, fixture.bindings, fixture.east_view, {}, context));
        pass.add(Submit::resolve_tilemap_layer(fixture.east, detail, fixture.bindings, fixture.east_view, {}, context));
    }
    expect_draws(older, authored);
    expect_packets(older, old_packets);
    const std::array edited{
        ExpectedDraw{12, 125, 191},
        ExpectedDraw{4, 165, 191},
        ExpectedDraw{8, 145, 179},
        ExpectedDraw{12, 145, 191},
        ExpectedDraw{4, 125, 179},
        authored[5],
        authored[6],
        authored[7]
    };
    expect_draws(newer, edited);
    EXPECT_EQ(cells(fixture.east), east_cells);
    const auto new_passes = newer.passes();
    ASSERT_EQ(new_passes.size(), 1u);
    ASSERT_EQ(new_passes[0].draws.size(), 8u);
    for (std::size_t i = 5; i < 8; ++i) {
        expect_payload(new_passes[0].draws[i], old_packets[i]);
        EXPECT_EQ(new_passes[0].draws[i].authored_order, old_packets[i].authored_order);
    }
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, resolver_failure) {
    Fixture fixture;
    Render::Frame frame;
    std::vector<Render::DrawPacket2D> prior;
    {
        Render::FrameWriter writer(frame);
        auto pass = writer.begin_pass(glm::mat4{1}, glm::mat4{1});
        const Submit::Context2D context{pass.semantics(), pass.parameters(), pass.constraints()};
        prior = Submit::resolve_tilemap_layer(fixture.east, ground, fixture.bindings, fixture.east_view, {}, context);
        ASSERT_EQ(prior.size(), 2u);
        pass.add(prior);
        const auto valid = Submit::resolve_tilemap_layer(fixture.west, ground, fixture.bindings, fixture.west_view, {}, context);
        ASSERT_EQ(valid.size(), 3u); // Same layer/view/style/context succeeded before the isolated map edit.
        fixture.west.set(ground, {1, 0}, {999});
        const auto before = fixture.calls->geometry_queries;
        // Isolate resolver rejection: no add operation is inside this assertion.
        EXPECT_THROW(
            static_cast<void>(Submit::resolve_tilemap_layer(fixture.west, ground, fixture.bindings, fixture.west_view, {}, context)),
            bad_request
        );
        EXPECT_GT(fixture.calls->geometry_queries, before); // The earlier valid cell reached packet validation.
    }
    expect_packets(frame, prior);
    const std::array expected{authored[5], authored[6]};
    expect_draws(frame, expected);
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, group_failure) {
    const Fixture fixture;
    Render::Frame frame;
    std::vector<Render::DrawPacket2D> prior;
    {
        Render::FrameWriter writer(frame);
        auto pass = writer.begin_pass(glm::mat4{1}, glm::mat4{1});
        const Submit::Context2D context{pass.semantics(), pass.parameters(), pass.constraints()};
        prior = Submit::resolve_tilemap_layer(fixture.west, detail, fixture.bindings, fixture.west_view, {}, context);
        ASSERT_EQ(prior.size(), 2u);
        pass.add(prior);
        auto group = Submit::resolve_tilemap_layer(fixture.east, ground, fixture.bindings, fixture.east_view, {}, context);
        ASSERT_EQ(group.size(), 2u);
        for (const auto& packet : group) {
            ASSERT_NO_THROW(Render::validate_draw_packet(packet, pass.constraints()));
        }
        ASSERT_EQ(group.back().vertex_count, 4u);
        group.back().vertex_count = 0; // Only the later packet's intended range field is invalidated.
        EXPECT_THROW(pass.add(std::move(group)), invalid_args);
    }
    expect_packets(frame, prior);
    const std::array expected{authored[3], authored[4]};
    expect_draws(frame, expected);
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, retention) {
    const auto calls = std::make_shared<Calls>();
    Render::Frame frame;
    std::vector<Render::DrawPacket2D> retained;
    std::weak_ptr<const Tiles::Asset> asset;
    std::weak_ptr<Geometry> geometry;
    std::weak_ptr<Image> image;
    std::weak_ptr<const Render::Resources::Material> material;
    std::weak_ptr<Pipeline> pipeline;
    {
        const Fixture fixture(calls);
        asset = fixture.asset;
        geometry = fixture.geometry;
        image = fixture.image;
        material = fixture.material;
        pipeline = fixture.pipeline;
        {
            Render::FrameWriter writer(frame);
            auto pass = writer.begin_pass(glm::mat4{1}, glm::mat4{1});
            const Submit::Context2D context{pass.semantics(), pass.parameters(), pass.constraints()};
            pass.add(Submit::resolve_tilemap_layer(fixture.west, ground, fixture.bindings, fixture.west_view, {}, context));
            pass.add(Submit::resolve_tilemap_layer(fixture.west, detail, fixture.bindings, fixture.west_view, {}, context));
            pass.add(Submit::resolve_tilemap_layer(fixture.east, ground, fixture.bindings, fixture.east_view, {}, context));
            pass.add(Submit::resolve_tilemap_layer(fixture.east, detail, fixture.bindings, fixture.east_view, {}, context));
        }
        calls->expect_cpu();
    } // Maps, bindings, provider and all fixture resource owners are gone.
    EXPECT_TRUE(asset.expired());
    ASSERT_FALSE(geometry.expired());
    ASSERT_FALSE(image.expired());
    ASSERT_FALSE(material.expired());
    ASSERT_FALSE(pipeline.expired());
    expect_draws(frame, authored);
    ASSERT_EQ(frame.passes().size(), 1u);
    retained = frame.passes()[0].draws; // Only now add a second owner to the frame's retained resources.
    expect_packets(frame, retained);
    calls->expect_cpu();
    frame.recycle(); // Synthetic resources only; all writers and borrowed frame views are gone.
    EXPECT_TRUE(frame.passes().empty());
    EXPECT_FALSE(geometry.expired()); // The explicit owned vector is still a resource owner.
    EXPECT_FALSE(image.expired());
    EXPECT_FALSE(material.expired());
    EXPECT_FALSE(pipeline.expired());
    calls->expect_cpu();
    retained.clear();
    EXPECT_TRUE(geometry.expired());
    EXPECT_TRUE(image.expired());
    EXPECT_TRUE(material.expired());
    EXPECT_TRUE(pipeline.expired());
    calls->expect_cpu();
}

TEST(tilemap_consumer, pick_paint_erase) {
    const auto calls = std::make_shared<Calls>();
    const ViewSample camera{{90, 170}, 2, {460, 48}};
    const std::array original{authored[0], authored[1], authored[2], authored[3], authored[4], authored[5]};
    const std::array changed{authored[0], ExpectedDraw{12, 125, 191}, authored[1], authored[2], authored[3], authored[4], authored[5]};
    Render::Frame older, painted, erased;
    std::vector<Render::DrawPacket2D> old_packets, painted_packets;
    std::weak_ptr<Geometry> geometry;
    std::weak_ptr<Image> image;
    {
        Fixture fixture(calls);
        geometry = fixture.geometry;
        image = fixture.image;
        const auto before = cells(fixture.west);
        const auto sibling = cells(fixture.east);
        prepare_view(older, fixture, camera);
        ASSERT_NO_FATAL_FAILURE(expect_draws(older, original));
        old_packets = older.passes()[0].draws;
        // Camera point (80,40) -> world (130,190) -> map-local (30,10).
        const auto picked = paint_at(fixture.west, ground, fixture.west_view, camera, {80, 40}, {10});
        ASSERT_EQ(picked, (Maps::Point{1, 0}));
        auto edited = before;
        edited[1] = {10};
        EXPECT_EQ(cells(fixture.west), edited); // All other cells and the other layer survive.
        EXPECT_EQ(cells(fixture.east), sibling);
        prepare_view(painted, fixture, camera);
        ASSERT_NO_FATAL_FAILURE(expect_draws(painted, changed));
        painted_packets = painted.passes()[0].draws;
        EXPECT_EQ(painted_packets[1].first_vertex, 12u); // New ground precedes the existing detail at the same cell.
        EXPECT_EQ(painted_packets[4].first_vertex, 12u);
        for (std::size_t i = 3; i < original.size(); ++i)
            expect_payload(painted_packets[i + 1], old_packets[i]); // Detail and sibling output differ only in assigned order.
        EXPECT_EQ(paint_at(fixture.west, ground, fixture.west_view, camera, {80, 40}, {}), picked);
        prepare_view(erased, fixture, camera);
        EXPECT_EQ(cells(fixture.west), before);
        EXPECT_EQ(cells(fixture.east), sibling);
        expect_packets(older, old_packets);
        expect_packets(painted, painted_packets);
        expect_packets(erased, old_packets);
        const auto passes = painted.passes();
        EXPECT_FLOAT_EQ(passes[0].view[0][0], 2);
        EXPECT_FLOAT_EQ(passes[0].view[3][0], -180);
        EXPECT_FLOAT_EQ(passes[0].view[3][1], -340);
        EXPECT_FLOAT_EQ(passes[0].projection[0][0], 2.0f / 460);
        EXPECT_FLOAT_EQ(passes[0].projection[1][1], 2.0f / 48);
        calls->expect_cpu();
    } // No frame observations overlap writers; maps/bindings/provider are now destroyed.
    expect_draws(older, original);
    expect_draws(painted, changed);
    expect_packets(older, old_packets);
    expect_packets(painted, painted_packets);
    expect_packets(erased, old_packets);
    EXPECT_FALSE(geometry.expired());
    EXPECT_FALSE(image.expired());
    calls->expect_cpu();
}

TEST(tilemap_consumer, pick_preservation) {
    Fixture fixture;
    const ViewSample camera{{90, 170}, 2, {460, 48}};
    const auto point = to_local(camera, fixture.west_view.origin, {80, 40});
    ASSERT_EQ(Maps::pick_cell(fixture.west.extent(), fixture.west_view.cell_size, point), (Maps::Point{1, 0}));
    const auto before = cells(fixture.west);
    const auto sibling = cells(fixture.east);
    Render::Frame older;
    prepare_view(older, fixture, camera);
    const auto packets = older.passes()[0].draws;
    const auto preserved = [&] {
        EXPECT_EQ(cells(fixture.west), before);
        EXPECT_EQ(cells(fixture.east), sibling);
        expect_packets(older, packets);
        fixture.calls->expect_cpu();
    };
    for (const Maps::LocalPoint outside : {Maps::LocalPoint{18, 40}, Maps::LocalPoint{140, 40}, Maps::LocalPoint{80, 12}}) {
        EXPECT_FALSE(paint_at(fixture.west, ground, fixture.west_view, camera, outside, {10}));
        preserved(); // Negative local X, exact right and exact bottom are ordinary misses.
    }
    const auto infinity = std::numeric_limits<double>::infinity();
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    const auto maximum = std::numeric_limits<double>::max();
    for (const auto zoom : {0.0, -1.0, infinity, -infinity, nan, maximum, std::numeric_limits<double>::denorm_min()}) {
        auto invalid = camera;
        invalid.zoom = zoom;
        EXPECT_THROW(static_cast<void>(paint_at(fixture.west, ground, fixture.west_view, invalid, {80, 40}, {10})), invalid_args);
        preserved();
    }
    for (const auto value : {nan, infinity, maximum}) {
        for (const auto member : {&Maps::LocalPoint::x, &Maps::LocalPoint::y}) {
            auto invalid = camera;
            invalid.pan.*member = value;
            EXPECT_THROW(static_cast<void>(paint_at(fixture.west, ground, fixture.west_view, invalid, {80, 40}, {10})), invalid_args);
            preserved();
        }
    }
    for (const auto origin : {glm::dvec2{nan, 200}, glm::dvec2{100, infinity}, glm::dvec2{maximum, 200}}) {
        auto invalid = fixture.west_view;
        invalid.origin = origin;
        EXPECT_THROW(static_cast<void>(paint_at(fixture.west, ground, invalid, camera, {80, 40}, {10})), invalid_args);
        preserved();
    }
    for (const auto picked : {Maps::LocalPoint{nan, 40}, Maps::LocalPoint{80, infinity}}) {
        EXPECT_THROW(static_cast<void>(paint_at(fixture.west, ground, fixture.west_view, camera, picked, {10})), invalid_args);
        preserved();
    }
    auto collapsed = camera;
    collapsed.pan.y = 0x1p60; // Both transformed viewport Y edges round together despite finite matrices.
    EXPECT_THROW(static_cast<void>(paint_at(fixture.west, ground, fixture.west_view, collapsed, {80, 40}, {10})), invalid_args);
    preserved();
    auto overflow = camera;
    overflow.zoom = 0.5;
    EXPECT_THROW(static_cast<void>(paint_at(fixture.west, ground, fixture.west_view, overflow, {maximum, 40}, {10})), invalid_args);
    preserved();
}

TEST(tilemap_consumer, pick_visibility) {
    const Fixture fixture;
    const ViewSample first{{90, 170}, 2, {460, 48}};
    const ViewSample shifted{{120, 174}, 0.5, {120, 12}};
    const auto one = to_local(first, fixture.west_view.origin, {80, 40});
    const auto two = to_local(shifted, fixture.west_view.origin, {5, 8});
    EXPECT_DOUBLE_EQ(one.x, 30);
    EXPECT_DOUBLE_EQ(one.y, 10);
    EXPECT_DOUBLE_EQ(two.x, 30);
    EXPECT_DOUBLE_EQ(two.y, 10);
    EXPECT_EQ(Maps::pick_cell(fixture.west.extent(), fixture.west_view.cell_size, one), (Maps::Point{1, 0}));
    EXPECT_EQ(Maps::pick_cell(fixture.west.extent(), fixture.west_view.cell_size, two), (Maps::Point{1, 0}));
    const auto visible = local_visible(shifted, fixture.west_view.origin);
    EXPECT_DOUBLE_EQ(visible.left, 20);
    EXPECT_DOUBLE_EQ(visible.top, 2);
    EXPECT_DOUBLE_EQ(visible.right, 260);
    EXPECT_DOUBLE_EQ(visible.bottom, 26);
    EXPECT_EQ(Maps::visible_cells(fixture.west.extent(), fixture.west_view.cell_size, visible), (Maps::CellRange{1, 3, 0, 2}));
    Render::Frame older, newer;
    prepare_view(older, fixture, first);
    const auto packets = older.passes()[0].draws;
    prepare_view(newer, fixture, shifted);
    const std::array expected{authored[1], authored[2], authored[3], authored[5], authored[6], authored[7]};
    expect_draws(newer, expected);
    expect_packets(older, packets);
    const auto passes = newer.passes();
    EXPECT_FLOAT_EQ(passes[0].view[0][0], 0.5f);
    EXPECT_FLOAT_EQ(passes[0].view[3][0], -60);
    EXPECT_FLOAT_EQ(passes[0].view[3][1], -87);
    fixture.calls->expect_cpu();
}

namespace {
    namespace Events = CE::Input::Events;
    using Key = CE::Input::KeyboardKey;
    using Mouse = CE::Input::MouseButton;
    const DemoTilemapEditor::Sizes editor_sizes{{400, 200}, {800, 600}}; // Independent 2x / 3x scale.

    Events::Record key(const Key code, const Events::ButtonPhase phase = Events::ButtonPhase::Press) {
        return {
            0,
            {},
            1,
            CE::Input::DeviceKind::Keyboard,
            Events::Button{CE::Input::control_id(code), phase, CE::Input::Modifiers::None, -1, -1, code}
        };
    }

    Events::Record click(const Mouse code, const Events::Pointer position, const Events::ButtonPhase phase = Events::ButtonPhase::Press) {
        return {
            0,
            {},
            2,
            CE::Input::DeviceKind::Mouse,
            Events::Button{CE::Input::control_id(code), phase, CE::Input::Modifiers::None, -1, -1, Key::Unknown, code, position}
        };
    }

    Events::Record wheel(const double y, const Events::Pointer position) {
        return {0, {}, 2, CE::Input::DeviceKind::Mouse, Events::Scroll{0, y, position}};
    }

    void edit(DemoTilemapEditor& editor, const std::initializer_list<Events::Record> records) {
        editor.consume({records.begin(), records.size()}, editor_sizes);
    }

    void prepare_editor(Render::Frame& frame, const DemoTilemapEditor& editor, const Fixture& fixture) {
        Render::FrameWriter writer(frame);
        Render::DrawStyle2D images;
        images.material = fixture.material;
        editor.write(writer, fixture.bindings, images);
    }
}

TEST(tilemap_consumer, editor_order) {
    const Fixture fixture;
    DemoTilemapEditor editor;
    edit(editor, {});
    const auto west = cells(editor.map(0));
    const auto east = cells(editor.map(1));
    ASSERT_EQ(editor.pick({72, 144}), (Maps::Point{1, 1})); // An initially empty ground cell.
    ASSERT_EQ(editor.pick({104, 144}), (Maps::Point{2, 1}));
    // Same simulation tick: these must use their OWN pointers, not the final
    // cursor or final axes. Release/repeat do not paint a third cell.
    edit(
        editor, {key(Key::Digit6),
                 click(Mouse::Left, {72, 144}),
                 click(Mouse::Left, {104, 144}),
                 click(Mouse::Left, {40, 128}, Events::ButtonPhase::Release),
                 click(Mouse::Left, {40, 128}, Events::ButtonPhase::Repeat),
                 {0, {}, 2, CE::Input::DeviceKind::Mouse, Events::Pointer{399, 199}},
                 {0, {}, 2, CE::Input::DeviceKind::Mouse, Events::Axis{1, 399}},
                 {0, {}, 2, CE::Input::DeviceKind::Mouse, Events::Axis{2, 199}}}
    );
    auto expected = west;
    expected[4] = expected[5] = {10};
    EXPECT_EQ(cells(editor.map(0)), expected);
    EXPECT_EQ(cells(editor.map(1)), east);
    EXPECT_EQ(editor.selected_map(), 0u);
    EXPECT_EQ(editor.selected_layer(), ground);
    EXPECT_EQ(editor.brush(), (Maps::TileId{10}));
    Render::Frame frame;
    prepare_editor(frame, editor, fixture);
    ASSERT_EQ(frame.passes().size(), 1u);
    const auto& draws = frame.passes()[0].draws;
    ASSERT_EQ(draws.size(), 16u);
    EXPECT_EQ(draws[4].first_vertex, 12u);
    EXPECT_EQ(draws[5].first_vertex, 12u);
    for (std::size_t i = 0; i < draws.size(); ++i)
        EXPECT_EQ(draws[i].authored_order, i);
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, editor_selection) {
    const Fixture fixture;
    DemoTilemapEditor editor;
    edit(editor, {});
    const auto west = cells(editor.map(0));
    const auto east = cells(editor.map(1));
    // Selection, paint, sample, selection, paint remain interleaved.
    edit(
        editor, {key(Key::Digit2), key(Key::T), key(Key::Digit5), click(Mouse::Left, {180, 140}), click(Mouse::Middle, {212, 156}),
                 key(Key::Digit1), key(Key::G), click(Mouse::Left, {72, 144})}
    );
    EXPECT_EQ(editor.brush(), (Maps::TileId{7})); // East detail (1,1), not ground or west.
    auto changed_east = east;
    changed_east[6] = {9};
    auto changed_west = west;
    changed_west[4] = {7};
    EXPECT_EQ(cells(editor.map(0)), changed_west);
    EXPECT_EQ(cells(editor.map(1)), changed_east);
    edit(editor, {click(Mouse::Right, {72, 144})});
    EXPECT_EQ(cells(editor.map(0)), west);
    EXPECT_EQ(cells(editor.map(1)), changed_east);
    // Empty sampling is intentional; it selects an erasing brush, not atlas zero.
    edit(editor, {click(Mouse::Middle, {72, 144}), click(Mouse::Left, {104, 144})});
    EXPECT_EQ(editor.brush(), (Maps::TileId{0}));
    EXPECT_EQ(editor.map(0).get(ground, {2, 1}), (Maps::TileId{0}));

    const auto before = cells(editor.map(0));
    auto blocked = click(Mouse::Left, {40, 128});
    blocked.to_gameplay = false;
    auto missing = click(Mouse::Left, {40, 128});
    std::get<Events::Button>(missing.data).position.reset();
    auto other_device = click(Mouse::Left, {40, 128});
    other_device.device_kind = CE::Input::DeviceKind::Other;
    auto blocked_key = key(Key::Digit2);
    blocked_key.to_gameplay = false;
    edit(
        editor, {blocked, missing, other_device, blocked_key, key(Key::Digit2, Events::ButtonPhase::Release),
                 key(Key::T, Events::ButtonPhase::Repeat), key(Key::Digit6, Events::ButtonPhase::Repeat)}
    );
    EXPECT_EQ(cells(editor.map(0)), before);
    EXPECT_EQ(cells(editor.map(1)), changed_east);
    EXPECT_EQ(editor.selected_map(), 0u);
    EXPECT_EQ(editor.selected_layer(), ground);
    EXPECT_EQ(editor.brush(), (Maps::TileId{0}));
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, editor_edges) {
    const Fixture fixture;
    DemoTilemapEditor editor;
    edit(editor, {});
    // FB = (2*x, 600-3*y). Top-left world (48,240); cells (64,48).
    EXPECT_EQ(editor.pick({24, 120}), (Maps::Point{0, 0}));
    EXPECT_EQ(editor.pick({56, 136}), (Maps::Point{1, 1}));
    const auto lower = [](const double n) { return std::nextafter(n, -std::numeric_limits<double>::infinity()); };
    const auto upper = [](const double n) { return std::nextafter(n, std::numeric_limits<double>::infinity()); };
    EXPECT_FALSE(editor.pick({lower(24), 128}));
    EXPECT_EQ(editor.pick({upper(24), 128}), (Maps::Point{0, 0}));
    EXPECT_EQ(editor.pick({lower(56), 128}), (Maps::Point{0, 0}));
    EXPECT_EQ(editor.pick({upper(56), 128}), (Maps::Point{1, 0}));
    EXPECT_FALSE(editor.pick({40, lower(120)}));
    EXPECT_EQ(editor.pick({40, upper(120)}), (Maps::Point{0, 0}));
    EXPECT_EQ(editor.pick({40, lower(136)}), (Maps::Point{0, 0}));
    EXPECT_EQ(editor.pick({40, upper(136)}), (Maps::Point{0, 1}));
    EXPECT_EQ(editor.pick({lower(120), 144}), (Maps::Point{2, 1}));
    EXPECT_FALSE(editor.pick({120, 144})); // Final right edge.
    EXPECT_FALSE(editor.pick({upper(120), 144}));
    EXPECT_EQ(editor.pick({40, lower(152)}), (Maps::Point{0, 1}));
    EXPECT_FALSE(editor.pick({40, 152})); // Final bottom edge.
    EXPECT_FALSE(editor.pick({40, upper(152)}));
    ASSERT_TRUE(editor.set_view({48, -360}, 1));
    EXPECT_EQ(editor.pick({0, 0}), (Maps::Point{0, 0})); // Logical top-left includes map top-left.
    EXPECT_FALSE(editor.pick({-1, 0}));
    EXPECT_FALSE(editor.pick({0, -1}));
    EXPECT_FALSE(editor.pick({400, 0}));
    EXPECT_FALSE(editor.pick({0, 200}));
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, editor_view) {
    const Fixture fixture;
    DemoTilemapEditor editor;
    edit(editor, {});
    const auto west = cells(editor.map(0));
    const auto east = cells(editor.map(1));
    edit(editor, {key(Key::D), key(Key::W), key(Key::D, Events::ButtonPhase::Repeat), key(Key::A, Events::ButtonPhase::Release)});
    EXPECT_DOUBLE_EQ(editor.pan().x, 32);
    EXPECT_DOUBLE_EQ(editor.pan().y, 32);
    edit(editor, {key(Key::A), key(Key::S)});
    EXPECT_DOUBLE_EQ(editor.pan().x, 0);
    EXPECT_DOUBLE_EQ(editor.pan().y, 0);
    ASSERT_EQ(editor.pick({40, 128}), (Maps::Point{0, 0}));
    edit(editor, {wheel(4, {40, 128}), {0, {}, 2, CE::Input::DeviceKind::Mouse, Events::Pointer{399, 199}}});
    EXPECT_DOUBLE_EQ(editor.zoom(), 2);
    EXPECT_DOUBLE_EQ(editor.pan().x, 40);
    EXPECT_DOUBLE_EQ(editor.pan().y, 108);
    EXPECT_EQ(editor.pick({40, 128}), (Maps::Point{0, 0})); // Cursor anchor held.
    edit(editor, {key(Key::R), wheel(4, {40, 128}), wheel(-4, {104, 144})});
    EXPECT_DOUBLE_EQ(editor.zoom(), 1);
    EXPECT_DOUBLE_EQ(editor.pan().x, -64); // Distinct wheel pointers in one tick.
    EXPECT_DOUBLE_EQ(editor.pan().y, 24);
    edit(editor, {key(Key::R)});
    EXPECT_DOUBLE_EQ(editor.zoom(), 1);
    EXPECT_DOUBLE_EQ(editor.pan().x, 0);
    EXPECT_DOUBLE_EQ(editor.pan().y, 0);
    Render::Frame older, shifted;
    prepare_editor(older, editor, fixture);
    const auto packets = older.passes()[0].draws;
    ASSERT_EQ(packets.size(), 15u);
    ASSERT_TRUE(editor.set_view({112, 144}, 2));
    EXPECT_EQ(editor.pick({0, 199}), (Maps::Point{1, 1}));
    prepare_editor(shifted, editor, fixture);
    const auto pass = shifted.passes();
    ASSERT_EQ(pass.size(), 1u);
    ASSERT_EQ(pass[0].draws.size(), 12u); // West col0 excluded at its exact edge.
    EXPECT_EQ(pass[0].draws[0].first_vertex, 4u);
    EXPECT_FLOAT_EQ(pass[0].projection[0][0], 2.0f / 800);
    EXPECT_FLOAT_EQ(pass[0].projection[1][1], 2.0f / 600);
    EXPECT_FLOAT_EQ(pass[0].view[0][0], 2);
    EXPECT_FLOAT_EQ(pass[0].view[3][0], -224);
    EXPECT_FLOAT_EQ(pass[0].view[3][1], -288);
    for (std::size_t i = 0; i < pass[0].draws.size(); ++i)
        EXPECT_EQ(pass[0].draws[i].authored_order, i);
    expect_packets(older, packets);
    EXPECT_EQ(cells(editor.map(0)), west);
    EXPECT_EQ(cells(editor.map(1)), east);
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, editor_invalid) {
    const Fixture fixture;
    DemoTilemapEditor editor;
    edit(editor, {});
    ASSERT_TRUE(editor.set_view({0, 0}, 1));
    ASSERT_EQ(editor.pick({72, 144}), (Maps::Point{1, 1}));
    const auto west = cells(editor.map(0));
    const auto east = cells(editor.map(1));
    const auto inf = std::numeric_limits<double>::infinity();
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    const auto max = std::numeric_limits<double>::max();
    for (const auto zoom : {0.0, -1.0, inf, -inf, nan, max, std::numeric_limits<double>::denorm_min()})
        EXPECT_FALSE(editor.set_view({0, 0}, zoom));
    for (const auto pan :
         {Maps::LocalPoint{nan, 0}, Maps::LocalPoint{0, inf}, Maps::LocalPoint{max, 0}, Maps::LocalPoint{0, -max},
          Maps::LocalPoint{0, 0x1p100}})
        EXPECT_FALSE(editor.set_view(pan, 1)); // Includes collapsed inverse viewport with finite float matrices.
    auto missing = wheel(4, {40, 128});
    std::get<Events::Scroll>(missing.data).position.reset();
    auto invalid_x = wheel(4, {40, 128});
    std::get<Events::Scroll>(invalid_x.data).x = nan;
    edit(
        editor,
        {wheel(max, {40, 128}), wheel(-max, {40, 128}), wheel(inf, {40, 128}), wheel(nan, {40, 128}), wheel(4, {nan, 128}),
         wheel(4, {40, inf}), wheel(4, {-1, 128}), wheel(4, {400, 128}), missing, invalid_x, click(Mouse::Left, {nan, 144}),
         click(Mouse::Right, {72, inf}), click(Mouse::Left, {120, 144}), click(Mouse::Left, {40, 152}), click(Mouse::Left, {-1, 128})}
    );
    EXPECT_DOUBLE_EQ(editor.pan().x, 0);
    EXPECT_DOUBLE_EQ(editor.pan().y, 0);
    EXPECT_DOUBLE_EQ(editor.zoom(), 1);
    EXPECT_EQ(cells(editor.map(0)), west);
    EXPECT_EQ(cells(editor.map(1)), east);
    EXPECT_EQ(editor.brush(), (Maps::TileId{7}));
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, editor_sizes) {
    const Fixture fixture;
    DemoTilemapEditor editor;
    EXPECT_FALSE(editor.pick({40, 128}));
    Render::Frame uninitialized;
    prepare_editor(uninitialized, editor, fixture);
    EXPECT_TRUE(uninitialized.passes().empty());
    edit(editor, {});
    ASSERT_TRUE(editor.set_view({32, 32}, 1));
    const auto west = cells(editor.map(0));
    const auto east = cells(editor.map(1));
    const std::array invalid{DemoTilemapEditor::Sizes{{0, 200}, {800, 600}},  DemoTilemapEditor::Sizes{{400, 0}, {800, 600}},
                             DemoTilemapEditor::Sizes{{-1, 200}, {800, 600}}, DemoTilemapEditor::Sizes{{400, -1}, {800, 600}},
                             DemoTilemapEditor::Sizes{{400, 200}, {0, 600}},  DemoTilemapEditor::Sizes{{400, 200}, {800, 0}},
                             DemoTilemapEditor::Sizes{{400, 200}, {-1, 600}}, DemoTilemapEditor::Sizes{{400, 200}, {800, -1}}};
    const std::array attempted{key(Key::Digit2), key(Key::T), key(Key::Digit6), key(Key::R), click(Mouse::Left, {40, 128})};
    for (const auto size : invalid) {
        editor.consume(attempted, size);
        EXPECT_FALSE(editor.pick({40, 128}));
        Render::Frame suspended;
        prepare_editor(suspended, editor, fixture);
        EXPECT_TRUE(suspended.passes().empty());
        EXPECT_DOUBLE_EQ(editor.pan().x, 32);
        EXPECT_DOUBLE_EQ(editor.pan().y, 32);
        EXPECT_EQ(editor.selected_map(), 0u);
        EXPECT_EQ(editor.selected_layer(), ground);
        EXPECT_EQ(editor.brush(), (Maps::TileId{7}));
    }
    EXPECT_EQ(cells(editor.map(0)), west);
    EXPECT_EQ(cells(editor.map(1)), east);
    edit(editor, {}); // A later captured positive size sample resumes preparation.
    Render::Frame restored;
    prepare_editor(restored, editor, fixture);
    ASSERT_EQ(restored.passes().size(), 1u);
    EXPECT_FLOAT_EQ(restored.passes()[0].view[3][0], -32);
    EXPECT_FLOAT_EQ(restored.passes()[0].view[3][1], -32);
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, editor_frames) {
    const auto calls = std::make_shared<Calls>();
    Render::Frame older, painted, erased;
    std::vector<Render::DrawPacket2D> original, changed;
    std::weak_ptr<Geometry> geometry;
    std::weak_ptr<Image> image;
    {
        const Fixture fixture(calls);
        DemoTilemapEditor editor;
        edit(editor, {});
        geometry = fixture.geometry;
        image = fixture.image;
        const auto west = cells(editor.map(0));
        const auto east = cells(editor.map(1));
        prepare_editor(older, editor, fixture);
        original = older.passes()[0].draws;
        ASSERT_EQ(original.size(), 15u);
        edit(editor, {key(Key::Digit6), click(Mouse::Left, {72, 144})});
        auto expected = west;
        expected[4] = {10};
        EXPECT_EQ(cells(editor.map(0)), expected);
        EXPECT_EQ(cells(editor.map(1)), east);
        prepare_editor(painted, editor, fixture);
        changed = painted.passes()[0].draws;
        ASSERT_EQ(changed.size(), 16u);
        EXPECT_EQ(changed[4].first_vertex, 12u); // Painted ground before details.
        EXPECT_FLOAT_EQ(std::get<float>(changed[4].parameters.at("alpha")), 1);
        EXPECT_FLOAT_EQ(std::get<float>(changed[6].parameters.at("alpha")), 0.5f);
        for (std::size_t i = 5; i < original.size(); ++i)
            expect_payload(changed[i + 1], original[i]); // Entire detail and sibling output unchanged apart from order.
        edit(editor, {click(Mouse::Right, {72, 144})});
        prepare_editor(erased, editor, fixture);
        EXPECT_EQ(cells(editor.map(0)), west);
        EXPECT_EQ(cells(editor.map(1)), east);
        expect_packets(older, original);
        expect_packets(painted, changed);
        expect_packets(erased, original);
        calls->expect_cpu();
    } // Includes editor maps/view/selection, bindings and fixture owners.
    expect_packets(older, original);
    expect_packets(painted, changed);
    expect_packets(erased, original);
    EXPECT_FALSE(geometry.expired());
    EXPECT_FALSE(image.expired());
    older.recycle();
    painted.recycle();
    erased.recycle();
    original.clear();
    EXPECT_FALSE(geometry.expired());
    EXPECT_FALSE(image.expired());
    changed.clear();
    EXPECT_TRUE(geometry.expired());
    EXPECT_TRUE(image.expired());
    calls->expect_cpu();
}

namespace {
    using TilemapTerrain::TerrainLabel;
    using TilemapTerrain::TerrainMap;

    Tiles::BitmaskRule terrain_rule() {
        Tiles::BitmaskRule rule;
        rule.type = Tiles::BitmaskType::FourNeighbor;
        rule.bit_order = {Tiles::Direction::North, Tiles::Direction::East, Tiles::Direction::South, Tiles::Direction::West};
        for (std::uint32_t mask = 0; mask < 16; ++mask)
            rule.cases.emplace(mask, mask % 4); // Synthetic selector targets, not terrain labels or logical IDs.
        return rule;
    }

    constexpr std::array terrain_ids{Maps::TileId{7}, Maps::TileId{8}, Maps::TileId{9}, Maps::TileId{10}};

    std::vector<TerrainLabel> terrain_labels(const TerrainMap& map) {
        std::vector<TerrainLabel> result;
        for (std::int64_t row = 0; row < map.tiles().extent().rows; ++row)
            for (std::int64_t column = 0; column < map.tiles().extent().columns; ++column)
                result.push_back(map.label({column, row}));
        return result;
    }

    void prepare_terrain(Render::Frame& frame, const Fixture& fixture, const TerrainMap& west, const TerrainMap& east) {
        Render::FrameWriter writer(frame);
        auto pass = writer.begin_pass(glm::mat4{1}, glm::mat4{1});
        const Submit::Context2D context{pass.semantics(), pass.parameters(), pass.constraints()};
        pass.add(Submit::resolve_tilemap_layer(west.tiles(), ground, fixture.bindings, fixture.west_view, {}, context));
        pass.add(Submit::resolve_tilemap_layer(west.tiles(), detail, fixture.bindings, fixture.west_view, {}, context));
        pass.add(Submit::resolve_tilemap_layer(east.tiles(), ground, fixture.bindings, fixture.east_view, {}, context));
        pass.add(Submit::resolve_tilemap_layer(east.tiles(), detail, fixture.bindings, fixture.east_view, {}, context));
    }
}

TEST(tilemap_consumer, terrain_frames) {
    const auto calls = std::make_shared<Calls>();
    Render::Frame older, newer;
    std::vector<Render::DrawPacket2D> prior, changed;
    std::weak_ptr<Geometry> geometry;
    std::weak_ptr<Image> image;
    const std::array original{
        ExpectedDraw{8, 105, 191},
        ExpectedDraw{0, 125, 191},
        ExpectedDraw{12, 105, 179},
        ExpectedDraw{4, 125, 179},
        authored[3],
        authored[4],
        ExpectedDraw{0, 305, 191},
        ExpectedDraw{0, 325, 179},
        authored[7]
    };
    const std::array edited{
        ExpectedDraw{0, 105, 191},
        ExpectedDraw{12, 105, 179},
        ExpectedDraw{0, 125, 179},
        authored[3],
        authored[4],
        ExpectedDraw{0, 305, 191},
        ExpectedDraw{0, 325, 179},
        authored[7]
    };
    {
        const Fixture fixture(calls);
        geometry = fixture.geometry;
        image = fixture.image;
        const std::vector<TerrainLabel> west_labels{{1}, {1}, {0}, {1}, {1}, {0}};
        const std::vector<TerrainLabel> east_labels{{2}, {0}, {0}, {0}, {2}, {0}};
        TerrainMap west(fixture.west, ground, west_labels, terrain_rule(), terrain_ids);
        const TerrainMap east(fixture.east, ground, east_labels, terrain_rule(), terrain_ids);
        const auto sibling = cells(east.tiles());
        const auto source_west = cells(fixture.west);
        auto expected = cells(west.tiles());
        prepare_terrain(older, fixture, west, east);
        ASSERT_NO_FATAL_FAILURE(expect_draws(older, original));
        prior = older.passes()[0].draws;
        EXPECT_EQ(west.edit({1, 0}, {0}), (std::vector<Maps::Point>{{1, 0}, {2, 0}, {1, 1}, {0, 0}}));
        expected[0] = {7};
        expected[1] = {0};
        expected[4] = {7};
        EXPECT_EQ(cells(west.tiles()), expected); // Includes untouched detail layer.
        EXPECT_EQ(cells(east.tiles()), sibling);
        EXPECT_EQ(terrain_labels(east), east_labels);
        EXPECT_EQ(cells(fixture.west), source_west); // Constructor never edited its caller's base.
        prepare_terrain(newer, fixture, west, east);
        ASSERT_NO_FATAL_FAILURE(expect_draws(newer, edited));
        changed = newer.passes()[0].draws;
        for (std::size_t i = 4; i < prior.size(); ++i)
            expect_payload(changed[i - 1], prior[i]); // Other layer/map retain their payloads; authored order shifts.
        expect_packets(older, prior);
        calls->expect_cpu();
    } // Terrain grids, rules/mappings, bindings and provider all gone.
    expect_draws(older, original);
    expect_draws(newer, edited);
    expect_packets(older, prior);
    expect_packets(newer, changed);
    EXPECT_FALSE(geometry.expired());
    EXPECT_FALSE(image.expired());
    older.recycle();
    newer.recycle();
    prior.clear();
    EXPECT_FALSE(geometry.expired());
    changed.clear();
    EXPECT_TRUE(geometry.expired());
    EXPECT_TRUE(image.expired());
    calls->expect_cpu();
}

TEST(tilemap_consumer, terrain_rejection) {
    const Fixture fixture;
    const std::vector<TerrainLabel> initial(6, {1});
    const TerrainMap east(fixture.east, ground, initial, terrain_rule(), terrain_ids);
    const auto sibling = cells(east.tiles());
    for (const auto missing_rule : {true, false}) {
        auto rule = terrain_rule();
        if (missing_rule)
            rule.cases.erase(2);
        else
            rule.cases[2] = std::numeric_limits<CE::Assets::Grids::CellIndex>::max();
        TerrainMap west(fixture.west, ground, initial, rule, terrain_ids); // Initial 3x2 masks never need case2.
        const auto before = cells(west.tiles());
        Render::Frame older, after;
        prepare_terrain(older, fixture, west, east);
        const auto packets = older.passes()[0].draws;
        ASSERT_EQ(packets.size(), 15u); // Six ground cells per map plus two west/one east detail draws.
        try {
            static_cast<void>(west.edit({0, 0}, {0})); // Empty center and east resolve first; south then refuses case2.
            ADD_FAILURE() << "Expected a late terrain derivation refusal";
        } catch (const bad_request& error) {
            EXPECT_NE(std::string{error.what()}.find("(0,1)"), std::string::npos);
        }
        EXPECT_EQ(terrain_labels(west), initial);
        EXPECT_EQ(cells(west.tiles()), before);
        EXPECT_EQ(cells(east.tiles()), sibling);
        prepare_terrain(after, fixture, west, east);
        expect_packets(older, packets);
        expect_packets(after, packets);
        fixture.calls->expect_cpu();
    }
}
