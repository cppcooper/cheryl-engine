#include "tilemap-editor.h"
#include "tilemap-demo.h"
#include "../../world-consumer/variable-documents.h"
#include "../../world-consumer/generated-expectations.h"
#include <cheryl/maps/terrain-map.h>

#include <assets/submission/tilemap2d.h>
#include <core/rendering/render-frame.h>

#include <assets/resources/resource-provider.h>
#include <gtest/gtest.h>
#include <internals/exceptions.h>

#include <ext/matrix_clip_space.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <span>
#include <sstream>
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
    using CE::Tilemaps::TerrainLabel;
    using CE::Tilemaps::TerrainMap;

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

TEST(tilemap_consumer, terrain_batch_frames) {
    const auto calls = std::make_shared<Calls>();
    Render::Frame older, newer;
    std::vector<Render::DrawPacket2D> prior, changed;
    std::vector<Maps::Point> affected;
    std::weak_ptr<Geometry> geometry;
    std::weak_ptr<Image> image;
    const std::array edited{
        ExpectedDraw{0, 105, 191},
        ExpectedDraw{8, 105, 179},
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
        const auto source = cells(fixture.west);
        prepare_terrain(older, fixture, west, east);
        prior = older.passes()[0].draws;
        ASSERT_EQ(prior.size(), 9u);
        {
            std::vector<Maps::TerrainEdit> requests{{{1, 0}, {0}}, {{0, 0}, {2}}, {{1, 0}, {0}}};
            affected = west.edit_batch(requests, {3, 5, 30}); // 3 requests + 10 visits + 5 resolutions + 12 samples.
            requests.assign(3, {{2, 1}, {99}});
        } // No request/temporary sampler lifetime survives the call.
        EXPECT_EQ(affected, (std::vector<Maps::Point>{{1, 0}, {2, 0}, {1, 1}, {0, 0}, {0, 1}}));
        EXPECT_EQ(terrain_labels(west), (std::vector<TerrainLabel>{{2}, {0}, {0}, {1}, {1}, {0}}));
        EXPECT_EQ(cells(east.tiles()), sibling);
        EXPECT_EQ(terrain_labels(east), east_labels);
        EXPECT_EQ(cells(fixture.west), source);
        EXPECT_EQ(west.tiles().get(detail, {0, 0}), fixture.west.get(detail, {0, 0}));
        prepare_terrain(newer, fixture, west, east);
        ASSERT_NO_FATAL_FAILURE(expect_draws(newer, edited));
        changed = newer.passes()[0].draws;
        for (std::size_t i = 4; i < prior.size(); ++i)
            expect_payload(changed[i - 1], prior[i]); // Old detail/sibling payloads keep their caller order.
        expect_packets(older, prior);
        calls->expect_cpu();
    }
    expect_packets(older, prior);
    expect_draws(newer, edited);
    EXPECT_FALSE(geometry.expired());
    EXPECT_FALSE(image.expired());
    older.recycle();
    newer.recycle();
    prior.clear();
    EXPECT_FALSE(geometry.expired());
    changed.clear();
    EXPECT_TRUE(geometry.expired());
    EXPECT_TRUE(image.expired());
    EXPECT_EQ(affected.front(), (Maps::Point{1, 0})); // Owned output survives maps and all inputs.
    calls->expect_cpu();
}

TEST(tilemap_consumer, terrain_batch_rejection) {
    const Fixture fixture;
    const std::vector<TerrainLabel> initial(6, {1});
    const TerrainMap east(fixture.east, ground, initial, terrain_rule(), terrain_ids);
    const auto sibling = cells(east.tiles());
    for (const auto failure : {0, 1, 2, 3, 4, 5, 6}) {
        auto rule = terrain_rule();
        std::vector<Maps::TileId> mapping(terrain_ids.begin(), terrain_ids.end());
        if (failure == 4)
            rule.cases.erase(2);
        else if (failure == 5)
            rule.cases[2] = std::numeric_limits<CE::Assets::Grids::CellIndex>::max();
        else if (failure == 6) {
            rule.cases[2] = mapping.size();
            mapping.push_back({0});
        }
        // Initial masks all exist/map; only the later requested derivation needs case2.
        TerrainMap west(fixture.west, ground, initial, rule, mapping);
        const auto before = cells(west.tiles());
        Render::Frame older, after;
        prepare_terrain(older, fixture, west, east);
        const auto packets = older.passes()[0].draws;
        ASSERT_EQ(packets.size(), 15u);
        std::array requests{Maps::TerrainEdit{{0, 0}, {0}}, Maps::TerrainEdit{{2, 1}, {0}}};
        Maps::TerrainBatchLimits limits{2, 6, 34};
        if (failure == 0)
            limits.requests = 1;
        else if (failure == 1)
            limits.affected_cells = 5;
        else if (failure == 2)
            limits.work = 33;
        else if (failure == 3)
            requests[1].point = {std::numeric_limits<std::int64_t>::max(), -1};
        EXPECT_THROW(static_cast<void>(west.edit_batch(requests, limits)), bad_request);
        EXPECT_EQ(terrain_labels(west), initial);
        EXPECT_EQ(cells(west.tiles()), before); // Every layer, not just the changed ground candidates.
        EXPECT_EQ(cells(east.tiles()), sibling);
        EXPECT_EQ(terrain_labels(east), initial);
        prepare_terrain(after, fixture, west, east);
        expect_packets(older, packets);
        expect_packets(after, packets);
        fixture.calls->expect_cpu();
    }
}

namespace {
    std::vector<TerrainLabel> editor_terrain_labels(const DemoTilemapEditor& editor) {
        std::vector<TerrainLabel> result;
        for (std::size_t instance = 0; instance < 2; ++instance)
            for (std::int64_t layer = 0; layer < 2; ++layer) {
                const auto values = terrain_labels(editor.terrain(instance, {layer}));
                result.insert(result.end(), values.begin(), values.end());
            }
        return result;
    }

    std::vector<Maps::TileId> editor_terrain_tiles(const DemoTilemapEditor& editor) {
        std::vector<Maps::TileId> result;
        for (std::size_t instance = 0; instance < 2; ++instance)
            for (std::int64_t layer = 0; layer < 2; ++layer)
                for (std::int64_t row = 0; row < 2; ++row)
                    for (std::int64_t column = 0; column < 3; ++column)
                        result.push_back(editor.terrain(instance, {layer}).tiles().get({layer}, {column, row}));
        return result;
    }

    // Independent full-board reference: literal adjacency comparisons and bit
    // weights, no Engine selector, batch traversal or cached visual connectivity.
    std::vector<Maps::TileId> diagnostic_tiles(const std::vector<TerrainLabel>& labels) {
        std::vector<Maps::TileId> result(24);
        for (std::size_t board = 0; board < 4; ++board)
            for (std::size_t row = 0; row < 2; ++row)
                for (std::size_t column = 0; column < 3; ++column) {
                    const auto index = board * 6 + row * 3 + column;
                    const auto center = labels.at(index);
                    if (!center.value)
                        continue;
                    const unsigned mask =
                        (row > 0 && labels.at(index - 3) == center ? 1 : 0) + (column < 2 && labels.at(index + 1) == center ? 2 : 0) +
                        (row < 1 && labels.at(index + 3) == center ? 4 : 0) + (column > 0 && labels.at(index - 1) == center ? 8 : 0);
                    result[index] = {7 + mask % 4};
                }
        return result;
    }
}

TEST(tilemap_consumer, editor_terrain_events) {
    const Fixture fixture;
    DemoTilemapEditor editor;
    const std::array literal{cells(editor.map(0)), cells(editor.map(1))};
    EXPECT_EQ(editor.mode(), DemoTilemapEditor::Mode::Literal);
    EXPECT_EQ(editor_terrain_labels(editor), std::vector<TerrainLabel>(24));
    edit(
        editor, {key(Key::B),
                 key(Key::Digit3),
                 click(Mouse::Left, {40, 128}),
                 click(Mouse::Left, {72, 128}),
                 click(Mouse::Left, {104, 128}, Events::ButtonPhase::Release),
                 click(Mouse::Left, {104, 128}, Events::ButtonPhase::Repeat),
                 {0, {}, 2, CE::Input::DeviceKind::Mouse, Events::Pointer{399, 199}},
                 {0, {}, 2, CE::Input::DeviceKind::Mouse, Events::Axis{1, 399}}}
    );
    std::vector<TerrainLabel> expected(24);
    expected[0] = expected[1] = {1};
    EXPECT_EQ(editor.mode(), DemoTilemapEditor::Mode::Terrain);
    EXPECT_EQ(editor_terrain_labels(editor), expected);
    EXPECT_EQ(editor_terrain_tiles(editor), diagnostic_tiles(expected));
    Render::Frame joined;
    prepare_editor(joined, editor, fixture);
    const std::array pair{ExpectedDraw{8, 64, 204}, ExpectedDraw{0, 128, 204}};
    expect_draws(joined, pair); // The first cell changes its target when its east neighbor is painted.

    edit(editor, {click(Mouse::Right, {72, 128}), click(Mouse::Middle, {40, 128}), key(Key::Digit4), click(Mouse::Left, {72, 128})});
    expected[1] = {2};
    EXPECT_EQ(editor_terrain_labels(editor), expected);
    EXPECT_EQ(editor_terrain_tiles(editor), diagnostic_tiles(expected)); // Different labels, both isolated visual ID 7.
    EXPECT_EQ(editor.terrain_brush(), (TerrainLabel{2}));
    edit(editor, {key(Key::Digit6), click(Mouse::Middle, {72, 128}), click(Mouse::Left, {104, 128})});
    EXPECT_EQ(editor.terrain_brush(), (TerrainLabel{2})); // Sample the label, never drawable ID 7.
    expected[2] = {2};
    EXPECT_EQ(editor_terrain_labels(editor), expected);
    EXPECT_EQ(editor_terrain_tiles(editor), diagnostic_tiles(expected));
    edit(editor, {click(Mouse::Middle, {104, 144}), click(Mouse::Left, {40, 128})});
    expected[0] = {0};
    EXPECT_EQ(editor.terrain_brush(), (TerrainLabel{0}));
    EXPECT_EQ(editor_terrain_labels(editor), expected);
    EXPECT_EQ(editor_terrain_tiles(editor), diagnostic_tiles(expected));
    EXPECT_EQ(cells(editor.map(0)), literal[0]);
    EXPECT_EQ(cells(editor.map(1)), literal[1]);
    EXPECT_EQ(editor.brush(), (Maps::TileId{7}));
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, editor_terrain_modes) {
    const Fixture fixture;
    DemoTilemapEditor editor;
    edit(editor, {key(Key::Digit6), click(Mouse::Left, {72, 144})});
    auto west_literal = cells(editor.map(0));
    const auto east_literal = cells(editor.map(1));
    Render::Frame literal;
    prepare_editor(literal, editor, fixture);
    const auto before = literal.passes()[0].draws;
    edit(
        editor, {key(Key::B), key(Key::Digit2), key(Key::T), key(Key::Digit5), click(Mouse::Left, {180, 140}),
                 click(Mouse::Middle, {212, 156}), key(Key::Digit1), key(Key::G), key(Key::Digit4), click(Mouse::Left, {40, 128}),
                 key(Key::T), click(Mouse::Left, {72, 144}), key(Key::V), click(Mouse::Left, {104, 144})}
    );
    std::vector<TerrainLabel> expected(24);
    expected[0] = expected[10] = {2};
    expected[18] = {3};
    EXPECT_EQ(editor_terrain_labels(editor), expected);
    EXPECT_EQ(editor_terrain_tiles(editor), diagnostic_tiles(expected));
    west_literal[11] = {10}; // Literal west detail (2,1), using its separate retained brush.
    EXPECT_EQ(cells(editor.map(0)), west_literal);
    EXPECT_EQ(cells(editor.map(1)), east_literal);
    EXPECT_EQ(editor.brush(), (Maps::TileId{10}));
    EXPECT_EQ(editor.terrain_brush(), (TerrainLabel{2}));
    EXPECT_EQ(editor.selected_map(), 0u);
    EXPECT_EQ(editor.selected_layer(), detail);
    edit(editor, {key(Key::B)});
    EXPECT_EQ(editor_terrain_labels(editor), expected);
    Render::Frame semantic;
    prepare_editor(semantic, editor, fixture);
    const std::array draws{ExpectedDraw{0, 64, 204}, ExpectedDraw{0, 128, 156}, ExpectedDraw{0, 344, 172}};
    expect_draws(semantic, draws); // West ground/detail, then east detail; explicit caller ordering.
    const auto& packets = semantic.passes()[0].draws;
    EXPECT_FLOAT_EQ(std::get<float>(packets[0].parameters.at("alpha")), 1);
    EXPECT_FLOAT_EQ(std::get<float>(packets[1].parameters.at("alpha")), 0.5);
    EXPECT_FLOAT_EQ(std::get<float>(packets[2].parameters.at("alpha")), 0.5);
    expect_packets(literal, before);
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, editor_terrain_view) {
    const Fixture fixture;
    DemoTilemapEditor editor;
    edit(
        editor, {key(Key::B), click(Mouse::Left, {56, 136}), click(Mouse::Left, {24, 120}), click(Mouse::Left, {120, 144}),
                 click(Mouse::Left, {40, 152}), key(Key::V, Events::ButtonPhase::Repeat)}
    );
    std::vector<TerrainLabel> expected(24);
    expected[0] = expected[4] = {1}; // Unequal 2x/3x framebuffer scale, exact internal edge and inclusive top-left.
    EXPECT_EQ(editor_terrain_labels(editor), expected);
    EXPECT_EQ(editor_terrain_tiles(editor), diagnostic_tiles(expected));
    const auto lower = [](const double n) { return std::nextafter(n, -std::numeric_limits<double>::infinity()); };
    const auto upper = [](const double n) { return std::nextafter(n, std::numeric_limits<double>::infinity()); };
    EXPECT_EQ(editor.pick({lower(56), 128}), (Maps::Point{0, 0}));
    EXPECT_EQ(editor.pick({upper(56), 128}), (Maps::Point{1, 0}));
    Render::Frame older;
    prepare_editor(older, editor, fixture);
    const auto old = older.passes()[0].draws;
    ASSERT_TRUE(editor.set_view({112, 144}, 2));
    ASSERT_EQ(editor.pick({0, 199}), (Maps::Point{1, 1}));
    edit(editor, {key(Key::Digit4), click(Mouse::Left, {0, 199})});
    expected[4] = {2};
    EXPECT_EQ(editor_terrain_labels(editor), expected);
    Render::Frame culled;
    prepare_editor(culled, editor, fixture);
    const std::array visible{ExpectedDraw{0, 128, 156}};
    expect_draws(culled, visible); // Col0 is exactly outside the camera-derived visible region.
    EXPECT_FLOAT_EQ(culled.passes()[0].view[0][0], 2);
    EXPECT_FLOAT_EQ(culled.passes()[0].view[3][0], -224);
    EXPECT_FALSE(editor.set_view({0, 0}, 0));
    EXPECT_FALSE(editor.set_view({std::numeric_limits<double>::infinity(), 0}, 1));
    const std::array attempted{key(Key::V), key(Key::Digit2), click(Mouse::Left, {40, 128})};
    for (const auto size :
         {DemoTilemapEditor::Sizes{{0, 200}, {800, 600}}, DemoTilemapEditor::Sizes{{400, 200}, {0, 600}},
          DemoTilemapEditor::Sizes{{400, 200}, {800, 0}}, DemoTilemapEditor::Sizes{{-1, 200}, {800, 600}}}) {
        editor.consume(attempted, size);
        EXPECT_EQ(editor.mode(), DemoTilemapEditor::Mode::Terrain);
        EXPECT_EQ(editor.selected_map(), 0u);
        EXPECT_EQ(editor_terrain_labels(editor), expected);
        Render::Frame suspended;
        prepare_editor(suspended, editor, fixture);
        EXPECT_TRUE(suspended.passes().empty());
    }
    edit(editor, {key(Key::R), wheel(4, {40, 128}), click(Mouse::Left, {40, 128})});
    expected[0] = {2};
    EXPECT_EQ(editor_terrain_labels(editor), expected);
    EXPECT_EQ(editor_terrain_tiles(editor), diagnostic_tiles(expected));
    EXPECT_DOUBLE_EQ(editor.zoom(), 2);
    EXPECT_DOUBLE_EQ(editor.pan().x, 40);
    EXPECT_DOUBLE_EQ(editor.pan().y, 108);
    edit(editor, {wheel(-4, {104, 144}), key(Key::V), key(Key::B)});
    EXPECT_DOUBLE_EQ(editor.zoom(), 1);
    EXPECT_DOUBLE_EQ(editor.pan().x, -64);
    EXPECT_DOUBLE_EQ(editor.pan().y, 24);
    expect_packets(older, old);
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, editor_terrain_failures) {
    const Fixture fixture;
    const std::array paint{
        key(Key::B),
        click(Mouse::Left, {40, 128}),
        click(Mouse::Left, {72, 128}),
        click(Mouse::Left, {104, 128}),
        click(Mouse::Left, {40, 144}),
        click(Mouse::Left, {72, 144}),
        click(Mouse::Left, {104, 144})
    };
    for (const auto failure : {0, 1, 2}) {
        auto palette = DemoTilemapEditor::diagnostic_terrain();
        if (failure == 0)
            palette.rule.cases.erase(2);
        else if (failure == 1)
            palette.rule.cases[2] = std::numeric_limits<CE::Assets::Grids::CellIndex>::max();
        else {
            palette.rule.cases[2] = 4;
            palette.target_to_id.push_back({0});
        }
        DemoTilemapEditor editor(palette);
        editor.consume(paint, editor_sizes); // Full initial masks never need case2; complete setup succeeds.
        const auto semantic = editor_terrain_labels(editor);
        const auto visual = editor_terrain_tiles(editor);
        ASSERT_EQ(semantic[0].value, 1u);
        Render::Frame older, after;
        prepare_editor(older, editor, fixture);
        const auto packets = older.passes()[0].draws;
        ASSERT_EQ(packets.size(), 6u);
        try {
            edit(editor, {click(Mouse::Right, {40, 128}), click(Mouse::Right, {104, 144})});
            ADD_FAILURE() << "Expected a late semantic input batch refusal";
        } catch (const bad_request& error) {
            EXPECT_NE(std::string(error.what()).find("(0,1)"), std::string::npos);
        }
        EXPECT_EQ(editor_terrain_labels(editor), semantic);
        EXPECT_EQ(editor_terrain_tiles(editor), visual);
        prepare_editor(after, editor, fixture);
        expect_packets(older, packets);
        expect_packets(after, packets);
    }
    for (const auto limits : {Maps::TerrainBatchLimits{1, 6, 256}, {2, 4, 256}, {2, 6, 24}}) {
        DemoTilemapEditor editor(DemoTilemapEditor::diagnostic_terrain(), limits);
        edit(editor, {key(Key::B), key(Key::Digit2), key(Key::T), click(Mouse::Left, {180, 140}), key(Key::Digit1), key(Key::G)});
        const auto semantic = editor_terrain_labels(editor); // Earlier completed sibling/layer run is retained.
        const auto visual = editor_terrain_tiles(editor);
        const std::array literal{cells(editor.map(0)), cells(editor.map(1))};
        Render::Frame older, after;
        prepare_editor(older, editor, fixture);
        const auto packets = older.passes()[0].draws;
        ASSERT_EQ(packets.size(), 1u);
        EXPECT_THROW(edit(editor, {click(Mouse::Left, {40, 128}), click(Mouse::Left, {72, 128})}), bad_request);
        EXPECT_EQ(editor_terrain_labels(editor), semantic);
        EXPECT_EQ(editor_terrain_tiles(editor), visual);
        EXPECT_EQ(cells(editor.map(0)), literal[0]);
        EXPECT_EQ(cells(editor.map(1)), literal[1]);
        prepare_editor(after, editor, fixture);
        expect_packets(older, packets);
        expect_packets(after, packets);
    }
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, editor_terrain_palette) {
    const Fixture fixture;
    auto palette = DemoTilemapEditor::diagnostic_terrain();
    DemoTilemapEditor owned(palette);
    palette.rule.cases.clear();
    palette.target_to_id.clear();
    edit(owned, {key(Key::B), click(Mouse::Left, {40, 128}), click(Mouse::Left, {72, 128})});
    EXPECT_EQ(editor_terrain_tiles(owned), diagnostic_tiles(editor_terrain_labels(owned)));

    auto aliases = DemoTilemapEditor::diagnostic_terrain();
    aliases.target_to_id.assign(4, {7});
    aliases.rule.cases.erase(2);
    DemoTilemapEditor shared_visual(aliases);
    edit(shared_visual, {key(Key::B), click(Mouse::Left, {40, 128}), key(Key::Digit4), click(Mouse::Left, {72, 128})});
    const auto semantic = editor_terrain_labels(shared_visual);
    const auto visual = editor_terrain_tiles(shared_visual);
    EXPECT_EQ(semantic[0], (TerrainLabel{1}));
    EXPECT_EQ(semantic[1], (TerrainLabel{2}));
    EXPECT_EQ(visual[0], (Maps::TileId{7}));
    EXPECT_EQ(visual[1], (Maps::TileId{7}));
    EXPECT_THROW(edit(shared_visual, {key(Key::Digit3), click(Mouse::Left, {72, 128})}), bad_request); // Equal label now needs mask2.
    EXPECT_EQ(editor_terrain_labels(shared_visual), semantic);
    EXPECT_EQ(editor_terrain_tiles(shared_visual), visual);
    edit(shared_visual, {click(Mouse::Middle, {72, 128})});
    EXPECT_EQ(shared_visual.terrain_brush(), (TerrainLabel{2}));

    for (const auto missing_rule : {false, true}) {
        auto missing = DemoTilemapEditor::diagnostic_terrain();
        missing.target_to_id.clear();
        if (missing_rule)
            missing.rule.cases.clear();
        DemoTilemapEditor empty(missing);
        const std::array literal{cells(empty.map(0)), cells(empty.map(1))};
        edit(empty, {key(Key::B), click(Mouse::Left, {40, 128}), click(Mouse::Right, {40, 128})}); // Cancel before any selection.
        EXPECT_EQ(editor_terrain_labels(empty), std::vector<TerrainLabel>(24));
        Render::Frame frame;
        prepare_editor(frame, empty, fixture);
        ASSERT_EQ(frame.passes().size(), 1u);
        EXPECT_TRUE(frame.passes()[0].draws.empty());
        EXPECT_THROW(edit(empty, {click(Mouse::Left, {40, 128})}), bad_request);
        EXPECT_EQ(editor_terrain_labels(empty), std::vector<TerrainLabel>(24));
        EXPECT_EQ(editor_terrain_tiles(empty), std::vector<Maps::TileId>(24));
        EXPECT_EQ(cells(empty.map(0)), literal[0]);
        EXPECT_EQ(cells(empty.map(1)), literal[1]);
    }
    auto invalid = DemoTilemapEditor::diagnostic_terrain();
    invalid.target_to_id[0] = {99};
    EXPECT_THROW(static_cast<void>(DemoTilemapEditor(invalid)), invalid_args);
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, editor_terrain_frames) {
    const auto calls = std::make_shared<Calls>();
    Render::Frame literal, painted, erased, returned;
    std::vector<Render::DrawPacket2D> original, changed, remaining;
    std::weak_ptr<Geometry> geometry;
    std::weak_ptr<Image> image;
    const std::array expected_painted{
        ExpectedDraw{8, 64, 204}, ExpectedDraw{0, 128, 204}, ExpectedDraw{0, 128, 204}, ExpectedDraw{0, 344, 172}
    };
    const std::array expected_erased{ExpectedDraw{0, 64, 204}, ExpectedDraw{0, 128, 204}, ExpectedDraw{0, 344, 172}};
    {
        const Fixture fixture(calls);
        geometry = fixture.geometry;
        image = fixture.image;
        DemoTilemapEditor editor;
        edit(editor, {});
        prepare_editor(literal, editor, fixture);
        original = literal.passes()[0].draws;
        ASSERT_EQ(original.size(), 15u);
        edit(
            editor, {key(Key::B), click(Mouse::Left, {40, 128}), click(Mouse::Left, {72, 128}), key(Key::T), click(Mouse::Left, {72, 128}),
                     key(Key::Digit2), key(Key::G), click(Mouse::Left, {180, 140})}
        );
        const auto painted_labels = editor_terrain_labels(editor);
        EXPECT_EQ(editor_terrain_tiles(editor), diagnostic_tiles(painted_labels));
        prepare_editor(painted, editor, fixture);
        ASSERT_NO_FATAL_FAILURE(expect_draws(painted, expected_painted));
        changed = painted.passes()[0].draws;
        edit(editor, {key(Key::Digit1), key(Key::G), click(Mouse::Right, {72, 128})});
        auto erased_labels = painted_labels;
        erased_labels[1] = {0};
        EXPECT_EQ(editor_terrain_labels(editor), erased_labels);
        EXPECT_EQ(editor_terrain_tiles(editor), diagnostic_tiles(erased_labels));
        prepare_editor(erased, editor, fixture);
        ASSERT_NO_FATAL_FAILURE(expect_draws(erased, expected_erased));
        remaining = erased.passes()[0].draws;
        edit(editor, {key(Key::V)});
        prepare_editor(returned, editor, fixture);
        expect_packets(returned, original);
        expect_packets(literal, original);
        expect_packets(painted, changed);
        calls->expect_cpu();
    } // All literal/semantic owners, events, bindings and provider gone.
    expect_packets(literal, original);
    expect_packets(returned, original);
    expect_packets(painted, changed);
    expect_packets(erased, remaining);
    EXPECT_FALSE(geometry.expired());
    EXPECT_FALSE(image.expired());
    literal.recycle();
    painted.recycle();
    erased.recycle();
    returned.recycle();
    original.clear();
    changed.clear();
    EXPECT_FALSE(geometry.expired());
    remaining.clear();
    EXPECT_TRUE(geometry.expired());
    EXPECT_TRUE(image.expired());
    calls->expect_cpu();
}

namespace {
    namespace Documents = CE::Maps;
    struct DocumentFiles {
        std::filesystem::path directory;

        DocumentFiles() {
            const auto root = std::filesystem::path(CHERYL_SOURCE_DIR) / "build/world-u2-checks";
            std::filesystem::create_directories(root); // Test-owned scratch only; the editor never creates directories.
            const auto nonce = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
            for (unsigned attempt = 0; attempt < 16; ++attempt) {
                directory = root / (nonce + "-" + std::to_string(attempt));
                if (std::filesystem::create_directory(directory)) {
                    std::cout << "Owned document fixtures: " << directory << '\n';
                    return;
                }
            }
            throw std::runtime_error("Could not create unique document test directory");
        }
        // Keep owned files as evidence. No earlier fixture/output is removed.
    };

    Documents::Snapshot custom_document() {
        return {
            "editor:authored",
            3,
            2,
            {{"floor", {"tilemap:brown", "tilemap:black", std::nullopt, "tilemap:red", std::nullopt, "tilemap:grey"}},
             {"trim", {std::nullopt, "tilemap:red", std::nullopt, "tilemap:grey", std::nullopt, std::nullopt}}},
            {{"arrival", "floor", 1, 0}, {"scenery", "trim", 0, 1}}
        };
    }
    void raw_document(const std::filesystem::path& path, const std::string_view text) {
        std::ofstream out(path, std::ios::binary);
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        out.close();
        if (out.fail())
            throw std::runtime_error("Test fixture write failed");
    }
    std::string document_bytes(const std::filesystem::path& path) {
        std::ifstream input(path, std::ios::binary);
        if (!input.is_open())
            throw std::runtime_error("Test fixture read failed");
        std::ostringstream result;
        result << input.rdbuf();
        if (input.bad() || result.fail())
            throw std::runtime_error("Incomplete test fixture read");
        return result.str();
    }
}

TEST(tilemap_consumer, editor_documents) {
    const Fixture fixture;
    const DocumentFiles files;
    Documents::save(files.directory / "west.json", custom_document());
    DemoTilemapEditor editor(files.directory);
    const auto east = editor.document(1).snapshot();
    edit(editor, {key(Key::T), key(Key::F9)});
    EXPECT_EQ(editor.document(0).snapshot(), custom_document()); // All ID/layer/spawn fields adopted.
    EXPECT_EQ(editor.selected_layer(), detail);                  // Selection remains an ordinal, names are retained metadata.
    Render::Frame older, restored;
    prepare_editor(older, editor, fixture);
    const auto previous = older.passes()[0].draws;
    edit(editor, {key(Key::Digit4), click(Mouse::Left, {104, 144}), key(Key::F8)});
    auto saved = custom_document();
    saved.layers[1].cells[5] = "tilemap:black";
    EXPECT_EQ(Documents::load(files.directory / "west.json"), saved);
    EXPECT_EQ(editor.document(0).snapshot(), saved);
    edit(editor, {click(Mouse::Right, {104, 144}), key(Key::F9)});
    EXPECT_EQ(editor.document(0).snapshot(), saved);
    EXPECT_EQ(editor.document(1).snapshot(), east);
    EXPECT_EQ(editor.mode(), DemoTilemapEditor::Mode::Literal);
    prepare_editor(restored, editor, fixture);
    EXPECT_EQ(restored.passes()[0].draws.size(), previous.size() + 1);
    expect_packets(older, previous);
    const auto west_bytes = document_bytes(files.directory / "west.json");
    edit(editor, {key(Key::Digit2), key(Key::G), key(Key::Digit6), click(Mouse::Left, {180, 140}), key(Key::F8)});
    auto east_saved = east;
    east_saved.layers[0].cells[0] = "tilemap:red";
    EXPECT_EQ(Documents::load(files.directory / "east.json"), east_saved);
    edit(editor, {click(Mouse::Right, {180, 140}), key(Key::F9)});
    EXPECT_EQ(editor.document(1).snapshot(), east_saved);
    EXPECT_EQ(editor.document(0).snapshot(), saved);
    EXPECT_EQ(document_bytes(files.directory / "west.json"), west_bytes);
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, editor_document_filters) {
    const DocumentFiles files;
    DemoTilemapEditor editor(files.directory);
    edit(editor, {key(Key::F8, Events::ButtonPhase::Release), key(Key::F9, Events::ButtonPhase::Repeat)});
    auto unadmitted = key(Key::F8);
    unadmitted.to_gameplay = false;
    const std::array denied{unadmitted};
    editor.consume(denied, editor_sizes);
    const std::array minimized{key(Key::F8)};
    editor.consume(minimized, {{0, 200}, {800, 600}});
    EXPECT_TRUE(std::filesystem::is_empty(files.directory)); // No constructor/update/release/repeat automatic I/O.
    edit(editor, {key(Key::F8)});
    const auto predecessor = document_bytes(files.directory / "west.json");
    edit(editor, {key(Key::B), click(Mouse::Left, {40, 128})});
    const auto labels = editor_terrain_labels(editor);
    const auto literal = editor.document(0).snapshot();
    for (const auto action : {Key::F8, Key::F9}) {
        EXPECT_NO_THROW(edit(editor, {key(action)}));
        EXPECT_EQ(editor.mode(), DemoTilemapEditor::Mode::Terrain);
        EXPECT_EQ(editor_terrain_labels(editor), labels);
        EXPECT_EQ(editor.document(0).snapshot(), literal);
        EXPECT_EQ(document_bytes(files.directory / "west.json"), predecessor);
    }
    DemoTilemapEditor disabled;
    const auto before = disabled.document(0).snapshot();
    EXPECT_THROW(edit(disabled, {key(Key::F8)}), bad_request);
    EXPECT_THROW(edit(disabled, {key(Key::F9)}), bad_request);
    EXPECT_EQ(disabled.document(0).snapshot(), before);
    DemoTilemapEditor missing(files.directory / "not-created");
    EXPECT_THROW(edit(missing, {key(Key::F8)}), bad_request);
    EXPECT_FALSE(std::filesystem::exists(files.directory / "not-created"));
}

TEST(tilemap_consumer, editor_document_load_failures) {
    const Fixture fixture;
    const DocumentFiles files;
    DemoTilemapEditor editor(files.directory);
    edit(editor, {key(Key::T), key(Key::Digit5), key(Key::D)});
    const auto before = editor.document(0).snapshot();
    const auto east = editor.document(1).snapshot();
    const auto* identity = &editor.document(0);
    Render::Frame old;
    prepare_editor(old, editor, fixture);
    const auto packets = old.passes()[0].draws;
    const auto preserved = [&] {
        EXPECT_EQ(&editor.document(0), identity);
        EXPECT_EQ(editor.document(0).snapshot(), before);
        EXPECT_EQ(editor.document(1).snapshot(), east);
        EXPECT_EQ(editor.selected_map(), 0u);
        EXPECT_EQ(editor.selected_layer(), detail);
        EXPECT_EQ(editor.mode(), DemoTilemapEditor::Mode::Literal);
        EXPECT_EQ(editor.brush(), (Maps::TileId{9}));
        EXPECT_DOUBLE_EQ(editor.pan().x, 32);
        expect_packets(old, packets);
    };
    EXPECT_THROW(edit(editor, {key(Key::F9)}), Documents::Error); // Missing file, existing directory.
    preserved();
    raw_document(files.directory / "west.json", "{");
    EXPECT_THROW(edit(editor, {key(Key::F9)}), Documents::Error);
    preserved();
    auto unknown = custom_document();
    unknown.layers[1].cells[5] = "tilemap:unknown";
    Documents::save(files.directory / "west.json", unknown); // Valid storage setup; conversion is the refusal.
    EXPECT_THROW(edit(editor, {key(Key::F9)}), WorldProof::Error);
    preserved();
    auto wrong_shape = custom_document();
    wrong_shape.width = 65;
    wrong_shape.height = 1;
    for (auto& layer : wrong_shape.layers)
        layer.cells.resize(65);
    wrong_shape.spawns = {{"entry", "floor", 64, 0}};
    Documents::save(files.directory / "west.json", wrong_shape);
    EXPECT_THROW(edit(editor, {key(Key::F9)}), bad_request);
    preserved();
    wrong_shape = custom_document();
    wrong_shape.layers.resize(1);
    wrong_shape.spawns.resize(1);
    Documents::save(files.directory / "west.json", wrong_shape);
    EXPECT_THROW(edit(editor, {key(Key::F9)}), bad_request);
    preserved();
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, editor_document_save_failures) {
    const Fixture fixture;
    const DocumentFiles files;
    DemoTilemapEditor editor(files.directory);
    edit(editor, {key(Key::F8)});
    const auto saved = editor.document(0).snapshot();
    const auto predecessor = document_bytes(files.directory / "west.json");
    // Inject one unsupported ID into an actually mutable Map via its read facade;
    // production controls emit only 7..10. This is a deliberate negative fixture.
    auto& actual = const_cast<Maps::Map&>(editor.map(0));
    actual.set(ground, {2, 1}, {999});
    EXPECT_THROW(edit(editor, {key(Key::F8)}), WorldProof::Error);
    EXPECT_EQ(actual.get(ground, {2, 1}), (Maps::TileId{999})); // No rollback of a prior authored runtime edit.
    EXPECT_EQ(document_bytes(files.directory / "west.json"), predecessor);
    EXPECT_EQ(Documents::load(files.directory / "west.json"), saved);
    actual.set(ground, {2, 1}, {7});
    // Native supported profile rejects a final symlink before file publication.
    std::filesystem::rename(files.directory / "west.json", files.directory / "predecessor.json");
    std::filesystem::create_symlink(files.directory / "predecessor.json", files.directory / "west.json");
    edit(editor, {key(Key::Digit6), click(Mouse::Left, {72, 144})});
    const auto authored = editor.document(0).snapshot();
    EXPECT_THROW(edit(editor, {key(Key::F8)}), Documents::Error);
    EXPECT_EQ(editor.document(0).snapshot(), authored);
    EXPECT_EQ(document_bytes(files.directory / "predecessor.json"), predecessor);
    EXPECT_EQ(Documents::load(files.directory / "predecessor.json"), saved);
    EXPECT_TRUE(std::filesystem::is_symlink(files.directory / "west.json"));
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, editor_document_frames) {
    const DocumentFiles files;
    Documents::save(files.directory / "west.json", custom_document());
    const auto calls = std::make_shared<Calls>();
    std::weak_ptr<Geometry> geometry;
    std::weak_ptr<Image> image;
    Render::Frame old, replaced;
    std::vector<Render::DrawPacket2D> original, updated;
    {
        const Fixture fixture(calls);
        geometry = fixture.geometry;
        image = fixture.image;
        DemoTilemapEditor editor(files.directory);
        edit(editor, {});
        prepare_editor(old, editor, fixture);
        original = old.passes()[0].draws;
        const auto owned_old = editor.document(0).snapshot();
        edit(editor, {key(Key::F9)});
        prepare_editor(replaced, editor, fixture);
        updated = replaced.passes()[0].draws;
        EXPECT_NE(owned_old, editor.document(0).snapshot());
        EXPECT_EQ(editor.document(0).snapshot(), custom_document());
        expect_packets(old, original);
        ASSERT_GE(original.size(), 3u);
        ASSERT_GE(updated.size(), 3u);
        EXPECT_NE(original[2].first_vertex, updated[2].first_vertex);
    }
    expect_packets(old, original);
    expect_packets(replaced, updated);
    EXPECT_FALSE(image.expired());
    EXPECT_FALSE(geometry.expired());
    old.recycle();
    replaced.recycle();
    original.clear();
    EXPECT_FALSE(image.expired());
    updated.clear();
    EXPECT_TRUE(image.expired());
    EXPECT_TRUE(geometry.expired());
    calls->expect_cpu();
}

TEST(tilemap_consumer, editor_document_diagnostics) {
    const DocumentFiles files;
    raw_document(files.directory / "west.json", "{");
    DemoTilemaps demo(files.directory); // No atlas load/native startup; exercise the real update catch only.
    const std::array bad_tick{key(Key::Digit6), click(Mouse::Left, {72, 144}), key(Key::F9), key(Key::Digit2), key(Key::F8)};
    testing::internal::CaptureStderr();
    demo.update(bad_tick, editor_sizes);
    const auto diagnostic = testing::internal::GetCapturedStderr();
    EXPECT_NE(diagnostic.find("document storage rejected"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(files.directory / "east.json")); // Rest of tick dropped.
    const std::array save{key(Key::F8)};
    demo.update(save, editor_sizes); // Later updates continue, preserving the earlier successful paint.
    const auto restored = Documents::load(files.directory / "west.json");
    EXPECT_EQ(restored.id, "tilemap:west");
    EXPECT_EQ(restored.layers[0].cells[4], "tilemap:red");
    auto unknown = custom_document();
    unknown.layers[1].cells[5] = "tilemap:unknown";
    Documents::save(files.directory / "west.json", unknown);
    testing::internal::CaptureStderr();
    const std::array load{key(Key::F9)};
    demo.update(load, editor_sizes);
    const auto conversion = testing::internal::GetCapturedStderr();
    EXPECT_NE(conversion.find("document conversion rejected"), std::string::npos);
    const std::array terrain{key(Key::B), key(Key::F9)};
    testing::internal::CaptureStderr();
    demo.update(terrain, editor_sizes);
    const auto mode = testing::internal::GetCapturedStderr();
    EXPECT_NE(mode.find("document storage rejected"), std::string::npos);
}

namespace {
    Documents::Snapshot semantic_document() {
        return {
            "terrain:authored",
            3,
            2,
            {{"soil", {"terrain:one", "terrain:one", "terrain:two", std::nullopt, "terrain:three", "terrain:three"}},
             {"accent", {"terrain:two", std::nullopt, "terrain:two", "terrain:four", std::nullopt, std::nullopt}}},
            {{"arrival", "soil", 1, 0}, {"scenery", "accent", 0, 1}, {"exit", "soil", 2, 1}}
        };
    }
    const std::array semantic_keys{
        WorldProof::KeyId{"terrain:one", {101}}, WorldProof::KeyId{"terrain:two", {203}}, WorldProof::KeyId{"terrain:three", {307}},
        WorldProof::KeyId{"terrain:four", {409}}
    };
    const std::array semantic_labels{
        WorldProof::SemanticKey{"terrain:one", {1}}, WorldProof::SemanticKey{"terrain:two", {2}},
        WorldProof::SemanticKey{"terrain:three", {3}}, WorldProof::SemanticKey{"terrain:four", {4}}
    };
    std::vector<TerrainLabel> semantic_expected() {
        std::vector<TerrainLabel> result(24);
        const std::array<unsigned, 12> west{1, 1, 2, 0, 3, 3, 2, 0, 2, 4, 0, 0};
        for (std::size_t i = 0; i < west.size(); ++i)
            result[i] = {west[i]};
        return result;
    }
}

TEST(tilemap_consumer, semantic_documents) {
    const Fixture fixture;
    const DocumentFiles files;
    Documents::save(files.directory / "west-terrain.json", semantic_document());
    DemoTilemapEditor editor(files.directory);
    edit(editor, {key(Key::B), key(Key::T), key(Key::F9)});
    const auto literal = editor.document(0).snapshot();
    const auto east = editor.terrain_document(1).snapshot();
    EXPECT_EQ(editor.terrain_document(0).snapshot(), semantic_document());
    EXPECT_EQ(editor_terrain_labels(editor), semantic_expected());
    EXPECT_EQ(editor_terrain_tiles(editor), diagnostic_tiles(semantic_expected()));
    // Labels 1 and 2 at ground (1,0)/(2,0) both select ID7, yet keys remain distinct.
    EXPECT_EQ(editor.terrain(0, ground).tiles().get(ground, {1, 0}), (Maps::TileId{7}));
    EXPECT_EQ(editor.terrain(0, ground).tiles().get(ground, {2, 0}), (Maps::TileId{7}));
    Render::Frame older, restored;
    prepare_editor(older, editor, fixture);
    const auto packets = older.passes()[0].draws;
    edit(
        editor,
        {key(Key::Digit4), click(Mouse::Left, {72, 144}), key(Key::G), key(Key::Digit6), click(Mouse::Left, {40, 144}), key(Key::F8)}
    );
    auto saved = semantic_document();
    saved.layers[1].cells[4] = "terrain:two";
    saved.layers[0].cells[3] = "terrain:four";
    EXPECT_EQ(Documents::load(files.directory / "west-terrain.json"), saved);
    edit(editor, {click(Mouse::Right, {40, 144}), key(Key::T), click(Mouse::Right, {72, 144}), key(Key::F9)});
    EXPECT_EQ(editor.terrain_document(0).snapshot(), saved);
    EXPECT_EQ(editor.selected_layer(), detail);
    EXPECT_EQ(editor.document(0).snapshot(), literal);
    EXPECT_EQ(editor.terrain_document(1).snapshot(), east);
    prepare_editor(restored, editor, fixture);
    expect_packets(older, packets);
    auto expected = semantic_expected();
    expected[3] = {4};
    expected[10] = {2};
    EXPECT_EQ(editor_terrain_labels(editor), expected);
    EXPECT_EQ(editor_terrain_tiles(editor), diagnostic_tiles(expected));
    const auto west_bytes = document_bytes(files.directory / "west-terrain.json");
    edit(
        editor, {key(Key::Digit2), key(Key::G), click(Mouse::Left, {180, 140}), key(Key::T), click(Mouse::Left, {212, 156}), key(Key::F8)}
    );
    const auto east_saved = editor.terrain_document(1).snapshot();
    EXPECT_EQ(east_saved.layers[0].cells[0], "terrain:four");
    EXPECT_EQ(east_saved.layers[1].cells[4], "terrain:four");
    edit(editor, {click(Mouse::Right, {212, 156}), key(Key::F9)});
    EXPECT_EQ(editor.terrain_document(1).snapshot(), east_saved);
    EXPECT_EQ(document_bytes(files.directory / "west-terrain.json"), west_bytes);
    edit(editor, {key(Key::V), key(Key::F8)});
    EXPECT_EQ(Documents::load(files.directory / "east.json"), editor.document(1).snapshot());
    EXPECT_EQ(editor.terrain_document(1).snapshot(), east_saved);
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, semantic_load_refusals) {
    const Fixture fixture;
    const DocumentFiles files;
    DemoTilemapEditor editor(files.directory);
    edit(editor, {key(Key::B), click(Mouse::Left, {40, 128}), key(Key::T), click(Mouse::Left, {72, 144})});
    const auto original = editor.terrain_document(0).snapshot();
    const auto east = editor.terrain_document(1).snapshot();
    const auto literal = editor.document(0).snapshot();
    const auto* identity = &editor.terrain_document(0);
    const auto visuals = editor_terrain_tiles(editor);
    Render::Frame older;
    prepare_editor(older, editor, fixture);
    const auto packets = older.passes()[0].draws;
    const auto preserved = [&] {
        EXPECT_EQ(&editor.terrain_document(0), identity);
        EXPECT_EQ(editor.terrain_document(0).snapshot(), original);
        EXPECT_EQ(editor.terrain_document(1).snapshot(), east);
        EXPECT_EQ(editor.document(0).snapshot(), literal);
        EXPECT_EQ(editor_terrain_tiles(editor), visuals);
        EXPECT_EQ(editor.mode(), DemoTilemapEditor::Mode::Terrain);
        EXPECT_EQ(editor.selected_map(), 0u);
        EXPECT_EQ(editor.selected_layer(), detail);
        expect_packets(older, packets);
    };
    EXPECT_THROW(edit(editor, {key(Key::F9)}), Documents::Error);
    preserved();
    raw_document(files.directory / "west-terrain.json", "{");
    EXPECT_THROW(edit(editor, {key(Key::F9)}), Documents::Error);
    preserved();
    for (const auto failure : {0, 1, 2, 3, 4}) {
        auto source = semantic_document();
        if (failure == 0)
            source.layers[1].cells[5] = "terrain:unknown";
        if (failure == 1)
            source.layers[1].cells[5] = "tilemap:red";
        if (failure == 2)
            source.id = "tilemap:west"; // Including all-null wrong-kind files.
        if (failure == 3) {
            source.width = 65;
            source.height = 1;
            for (auto& layer : source.layers)
                layer.cells.resize(65);
            source.spawns.clear();
        }
        if (failure == 4) {
            source.layers.resize(1);
            source.spawns.clear();
        }
        Documents::save(files.directory / "west-terrain.json", source);
        if (failure >= 3)
            EXPECT_THROW(edit(editor, {key(Key::F9)}), bad_request);
        else
            EXPECT_THROW(edit(editor, {key(Key::F9)}), WorldProof::Error);
        preserved();
    }
    auto nulls = semantic_document();
    for (auto& layer : nulls.layers)
        for (auto& cell : layer.cells)
            cell.reset();
    Documents::save(files.directory / "west.json", nulls);
    edit(editor, {key(Key::V)});
    EXPECT_THROW(edit(editor, {key(Key::F9)}), WorldProof::Error);
    EXPECT_EQ(editor.document(0).snapshot(), literal);
    nulls.id = "tilemap:west";
    Documents::save(files.directory / "west-terrain.json", nulls);
    edit(editor, {key(Key::B)});
    EXPECT_THROW(edit(editor, {key(Key::F9)}), WorldProof::Error);
    preserved();
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, semantic_atomic_layers) {
    const Fixture fixture;
    const DocumentFiles files;
    for (const auto failure : {0, 1, 2}) {
        auto palette = DemoTilemapEditor::diagnostic_terrain();
        if (failure == 0)
            palette.rule.cases.erase(2);
        if (failure == 1)
            palette.rule.cases[2] = 100;
        if (failure == 2) {
            palette.rule.cases[2] = 4;
            palette.target_to_id.push_back({0});
        }
        DemoTilemapEditor editor(palette, {32, 6, 256}, files.directory);
        edit(editor, {key(Key::B), key(Key::Digit4), click(Mouse::Left, {104, 144})});
        const auto original = editor.terrain_document(0).snapshot();
        const auto labels = editor_terrain_labels(editor);
        const auto visual = editor_terrain_tiles(editor);
        const auto* identity = &editor.terrain_document(0);
        auto source = semantic_document();
        source.layers[0].cells = {"terrain:four", std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
        source.layers[1].cells = {"terrain:one", "terrain:one", std::nullopt, std::nullopt, std::nullopt, std::nullopt};
        const WorldProof::KeyTable keys(semantic_keys);
        // Prove first replacement owner succeeds, but second requires missing/unmapped mask2.
        const auto first = WorldProof::TerrainOwner::from(source, keys, ground, semantic_labels, palette.rule, palette.target_to_id);
        ASSERT_EQ(first->terrain().label({0, 0}), (TerrainLabel{4}));
        Documents::save(files.directory / "west-terrain.json", source);
        Render::Frame older, after;
        prepare_editor(older, editor, fixture);
        const auto packets = older.passes()[0].draws;
        try {
            edit(editor, {key(Key::F9)});
            ADD_FAILURE() << "Expected second-layer failure";
        } catch (const bad_request& error) {
            EXPECT_NE(std::string(error.what()).find("(0,0)"), std::string::npos);
        }
        EXPECT_EQ(&editor.terrain_document(0), identity);
        EXPECT_EQ(editor.terrain_document(0).snapshot(), original);
        EXPECT_EQ(editor_terrain_labels(editor), labels);
        EXPECT_EQ(editor_terrain_tiles(editor), visual);
        prepare_editor(after, editor, fixture);
        expect_packets(after, packets);
        expect_packets(older, packets);
    }
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, semantic_tables) {
    const WorldProof::KeyTable keys(semantic_keys);
    auto palette = DemoTilemapEditor::diagnostic_terrain();
    auto live = WorldProof::TerrainDocument::from(semantic_document(), keys, semantic_labels, palette.rule, palette.target_to_id);
    const auto* identity = live.get();
    const auto original = live->snapshot();
    for (const auto failure : {0, 1, 2, 3, 4, 5}) {
        auto table = std::vector<WorldProof::SemanticKey>(semantic_labels.begin(), semantic_labels.end());
        if (failure == 0)
            table[3].key = "terrain:unknown";
        if (failure == 1)
            table[3].key = table[0].key;
        if (failure == 2)
            table[3].label = table[0].label;
        if (failure == 3)
            table[3].label = {0};
        if (failure == 4)
            table.pop_back(); // Known numeric key, absent semantic mapping in SECOND layer.
        if (failure == 5)
            table.clear();
        EXPECT_THROW(
            live = WorldProof::TerrainDocument::from(semantic_document(), keys, table, palette.rule, palette.target_to_id),
            WorldProof::Error
        );
        EXPECT_EQ(live.get(), identity);
        EXPECT_EQ(live->snapshot(), original);
    }
    for (const auto& table :
         {std::vector<WorldProof::KeyId>{{"terrain:one", {0}}},
          {{"terrain:one", {101}}, {"terrain:one", {203}}},
          {{"terrain:one", {101}}, {"terrain:two", {101}}}}) {
        EXPECT_THROW(static_cast<void>(WorldProof::KeyTable(table)), WorldProof::Error);
    }
    auto held = original;
    const std::array unknown{Maps::TerrainEdit{{0, 0}, {99}}, Maps::TerrainEdit{{0, 0}, {1}}};
    EXPECT_THROW(static_cast<void>(live->edit_batch(ground, unknown, {2, 5, 100})), WorldProof::Error);
    EXPECT_THROW(static_cast<void>(live->terrain({-1})), WorldProof::Error);
    EXPECT_THROW(static_cast<void>(live->edit_batch({2}, {}, {0, 0, 0})), WorldProof::Error);
    EXPECT_EQ(live->snapshot(), held);
    // No retained temporary snapshot/table/rule/target storage, even for future replacements.
    {
        auto input = semantic_document();
        auto meanings = std::vector<WorldProof::SemanticKey>(semantic_labels.begin(), semantic_labels.end());
        auto ids = std::vector<WorldProof::KeyId>(semantic_keys.begin(), semantic_keys.end());
        const WorldProof::KeyTable local(ids);
        live = WorldProof::TerrainDocument::from(input, local, meanings, palette.rule, palette.target_to_id);
        input.layers.clear();
        ids.clear();
        meanings.clear();
        palette.rule.cases.clear();
        palette.target_to_id.clear();
    }
    auto replacement = live->replacement(semantic_document());
    EXPECT_EQ(replacement->snapshot(), held);
    const std::array edit_one{Maps::TerrainEdit{{2, 1}, {4}}};
    static_cast<void>(replacement->edit_batch(detail, edit_one, {1, 5, 100}));
    held.layers[1].cells[5] = "terrain:four";
    EXPECT_EQ(replacement->snapshot(), held);
    EXPECT_EQ(live->snapshot(), original);
}

TEST(tilemap_consumer, semantic_actions) {
    const DocumentFiles files;
    DemoTilemapEditor editor(files.directory);
    edit(editor, {key(Key::B)});
    auto unadmitted = key(Key::F8);
    unadmitted.to_gameplay = false;
    const std::array ignored{key(Key::F8, Events::ButtonPhase::Release), key(Key::F9, Events::ButtonPhase::Repeat), unadmitted};
    editor.consume(ignored, editor_sizes);
    const std::array save{key(Key::F8)};
    editor.consume(save, {{400, 200}, {0, 600}});
    EXPECT_TRUE(std::filesystem::is_empty(files.directory));
    edit(editor, {click(Mouse::Left, {40, 128}), key(Key::F8)});
    const auto path = files.directory / "west-terrain.json";
    const auto original = editor.terrain_document(0).snapshot();
    const auto before = document_bytes(path);
    std::filesystem::rename(path, files.directory / "predecessor.json");
    std::filesystem::create_symlink(files.directory / "predecessor.json", path);
    edit(editor, {key(Key::T), key(Key::Digit4), click(Mouse::Left, {72, 144})});
    const auto authored = editor.terrain_document(0).snapshot();
    EXPECT_THROW(edit(editor, {key(Key::F8)}), Documents::Error);
    EXPECT_EQ(editor.terrain_document(0).snapshot(), authored);
    EXPECT_EQ(document_bytes(files.directory / "predecessor.json"), before);
    EXPECT_EQ(Documents::load(files.directory / "predecessor.json"), original);
    EXPECT_TRUE(std::filesystem::is_symlink(path));
    DemoTilemapEditor disabled;
    edit(disabled, {key(Key::B)});
    EXPECT_THROW(edit(disabled, {key(Key::F8)}), bad_request);
    EXPECT_THROW(edit(disabled, {key(Key::F9)}), bad_request);
    DemoTilemapEditor missing(files.directory / "not-created");
    edit(missing, {key(Key::B)});
    EXPECT_THROW(edit(missing, {key(Key::F8)}), bad_request);
    EXPECT_FALSE(std::filesystem::exists(files.directory / "not-created"));
    // Exercise the SAME update diagnostics/drop policy actually used by demo.
    DemoTilemaps demo(files.directory);
    auto bad = semantic_document();
    bad.layers[1].cells[5] = "terrain:unknown";
    Documents::save(files.directory / "east-terrain.json", bad);
    const std::array tick{key(Key::B),  key(Key::Digit2), key(Key::T), key(Key::Digit5), click(Mouse::Left, {212, 156}),
                          key(Key::F9), key(Key::Digit1), key(Key::F8)};
    testing::internal::CaptureStderr();
    demo.update(tick, editor_sizes);
    const auto diagnostic = testing::internal::GetCapturedStderr();
    EXPECT_NE(diagnostic.find("document conversion rejected"), std::string::npos);
    EXPECT_NE(diagnostic.find("remaining tick records dropped"), std::string::npos);
    demo.update(save, editor_sizes); // Later update persists earlier semantic edit, still selected east/detail.
    const auto saved = Documents::load(files.directory / "east-terrain.json");
    EXPECT_EQ(saved.id, "terrain:east");
    EXPECT_EQ(saved.layers[1].cells[4], "terrain:three");
    EXPECT_EQ(document_bytes(files.directory / "predecessor.json"), before);
}

TEST(tilemap_consumer, semantic_document_frames) {
    const auto calls = std::make_shared<Calls>();
    const DocumentFiles files;
    Render::Frame older, replaced;
    std::vector<Render::DrawPacket2D> before, after;
    std::weak_ptr<Image> image;
    std::weak_ptr<Geometry> geometry;
    {
        const Fixture fixture(calls);
        image = fixture.image;
        geometry = fixture.geometry;
        DemoTilemapEditor editor(files.directory);
        edit(editor, {key(Key::B), click(Mouse::Left, {40, 128}), key(Key::T), click(Mouse::Left, {72, 144}), key(Key::F8)});
        prepare_editor(older, editor, fixture);
        before = older.passes()[0].draws;
        ASSERT_EQ(before.size(), 2u);
        const auto held = editor.terrain_document(0).snapshot();
        Documents::save(files.directory / "west-terrain.json", semantic_document());
        edit(editor, {key(Key::F9)});
        EXPECT_NE(editor.terrain_document(0).snapshot(), held);
        prepare_editor(replaced, editor, fixture);
        after = replaced.passes()[0].draws;
        ASSERT_EQ(after.size(), 8u);
        EXPECT_EQ(editor_terrain_tiles(editor), diagnostic_tiles(semantic_expected()));
        expect_packets(older, before);
        for (std::size_t i = 0; i < after.size(); ++i)
            EXPECT_EQ(after[i].authored_order, i);
        calls->expect_cpu();
    }
    expect_packets(older, before);
    expect_packets(replaced, after);
    EXPECT_FALSE(image.expired());
    EXPECT_FALSE(geometry.expired());
    older.recycle();
    replaced.recycle();
    before.clear();
    EXPECT_FALSE(image.expired());
    after.clear();
    EXPECT_TRUE(image.expired());
    EXPECT_TRUE(geometry.expired());
    calls->expect_cpu();
}

namespace {
    using Mode = DemoTilemapEditor::Mode;
    using HistoryState = WorldProof::HistoryState;
    std::size_t history_cost(const Documents::Snapshot& before, const Documents::Snapshot& after) {
        return Documents::encode(before).size() + Documents::encode(after).size();
    }
    void expect_semantic_history(
        const DemoTilemapEditor& editor,
        const Documents::Snapshot& west,
        const Documents::Snapshot& east,
        const std::vector<TerrainLabel>& labels
    ) {
        EXPECT_EQ(editor.terrain_document(0).snapshot(), west);
        EXPECT_EQ(editor.terrain_document(1).snapshot(), east);
        EXPECT_EQ(editor_terrain_labels(editor), labels);
        EXPECT_EQ(editor_terrain_tiles(editor), diagnostic_tiles(labels));
    }
}

TEST(tilemap_consumer, history_literal) {
    const Fixture fixture;
    const DocumentFiles files;
    Documents::save(files.directory / "west.json", custom_document());
    DemoTilemapEditor editor(files.directory);
    edit(editor, {key(Key::F9)});
    const auto base = custom_document();
    const auto east = editor.document(1).snapshot();
    const auto semantic = editor.terrain_document(0).snapshot();
    EXPECT_EQ(editor.history(0, Mode::Literal), HistoryState{});
    EXPECT_THROW(edit(editor, {key(Key::F6)}), WorldProof::Error);
    EXPECT_THROW(edit(editor, {key(Key::F7)}), WorldProof::Error);
    EXPECT_EQ(editor.document(0).snapshot(), base);
    edit(editor, {key(Key::Digit6), click(Mouse::Left, {72, 144})});
    auto painted = base;
    painted.layers[0].cells[4] = "tilemap:red";
    EXPECT_EQ(editor.document(0).snapshot(), painted);
    EXPECT_EQ(editor.history(0, Mode::Literal), (HistoryState{1, 0, history_cost(base, painted)}));
    edit(editor, {key(Key::T), click(Mouse::Right, {72, 128})});
    auto erased = painted;
    erased.layers[1].cells[1].reset();
    EXPECT_EQ(editor.document(0).snapshot(), erased);
    const auto retained = history_cost(base, painted) + history_cost(painted, erased);
    EXPECT_EQ(editor.history(0, Mode::Literal), (HistoryState{2, 0, retained}));
    edit(editor, {key(Key::D), key(Key::F6)});
    EXPECT_EQ(editor.document(0).snapshot(), painted);
    EXPECT_EQ(editor.selected_layer(), detail);
    EXPECT_DOUBLE_EQ(editor.pan().x, 32);
    EXPECT_EQ(editor.history(0, Mode::Literal), (HistoryState{1, 1, retained}));
    edit(editor, {key(Key::F6)});
    EXPECT_EQ(editor.document(0).snapshot(), base);
    EXPECT_EQ(editor.history(0, Mode::Literal), (HistoryState{0, 2, retained}));
    // No-op erase leaves redo intact; view/brush/layer are not restored by undo.
    edit(editor, {key(Key::R), click(Mouse::Right, {104, 128})});
    EXPECT_EQ(editor.document(0).snapshot(), base);
    EXPECT_EQ(editor.history(0, Mode::Literal), (HistoryState{0, 2, retained}));
    edit(editor, {key(Key::F7)});
    EXPECT_EQ(editor.document(0).snapshot(), painted);
    edit(editor, {key(Key::F7)});
    EXPECT_EQ(editor.document(0).snapshot(), erased);
    edit(editor, {key(Key::F6), key(Key::Digit2), key(Key::G), key(Key::Digit4), click(Mouse::Left, {180, 140})});
    auto edited_east = east;
    edited_east.layers[0].cells[0] = "tilemap:black";
    EXPECT_EQ(editor.document(1).snapshot(), edited_east);
    edit(editor, {key(Key::B), key(Key::Digit1), click(Mouse::Left, {40, 128})});
    EXPECT_EQ(editor.history(0, Mode::Terrain).undo, 1u);
    EXPECT_EQ(editor.history(0, Mode::Literal), (HistoryState{1, 1, retained}));
    edit(editor, {key(Key::V), key(Key::T), key(Key::Digit5), click(Mouse::Left, {104, 144})});
    auto branched = painted;
    branched.layers[1].cells[5] = "tilemap:grey";
    EXPECT_EQ(editor.document(0).snapshot(), branched);
    EXPECT_EQ(editor.history(0, Mode::Literal).undo, 2u);
    EXPECT_EQ(editor.history(0, Mode::Literal).redo, 0u);
    EXPECT_THROW(edit(editor, {key(Key::F7)}), WorldProof::Error);
    EXPECT_EQ(editor.document(1).snapshot(), edited_east);
    edit(editor, {key(Key::B), key(Key::F6)});
    EXPECT_EQ(editor.terrain_document(0).snapshot(), semantic);
    EXPECT_EQ(editor.document(0).snapshot(), branched);
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, history_terrain) {
    const Fixture fixture;
    DemoTilemapEditor editor;
    const auto base = editor.terrain_document(0).snapshot();
    const auto east = editor.terrain_document(1).snapshot();
    const auto literal = editor.document(0).snapshot();
    std::vector<TerrainLabel> labels(24);
    edit(
        editor, {key(Key::B), click(Mouse::Left, {40, 128}), click(Mouse::Left, {72, 128}), click(Mouse::Right, {40, 128}),
                 click(Mouse::Left, {40, 128})}
    );
    auto ground_doc = base;
    ground_doc.layers[0].cells[0] = ground_doc.layers[0].cells[1] = "terrain:one";
    labels[0] = labels[1] = {1};
    expect_semantic_history(editor, ground_doc, east, labels);
    const auto ground_cost = history_cost(base, ground_doc);
    EXPECT_EQ(editor.history(0, Mode::Terrain), (HistoryState{1, 0, ground_cost})); // Four samples, ONE accepted batch.
    edit(editor, {key(Key::T), key(Key::Digit4), click(Mouse::Left, {40, 128})});
    auto both = ground_doc;
    both.layers[1].cells[0] = "terrain:two";
    labels[6] = {2};
    expect_semantic_history(editor, both, east, labels);
    const auto both_cost = ground_cost + history_cost(ground_doc, both);
    EXPECT_EQ(editor.history(0, Mode::Terrain), (HistoryState{2, 0, both_cost}));
    edit(editor, {key(Key::F6)});
    labels[6] = {0};
    expect_semantic_history(editor, ground_doc, east, labels);
    EXPECT_EQ(editor.selected_layer(), detail);
    // Coalesced cancelled run and redundant ground paint are no commands, preserving redo.
    edit(
        editor,
        {click(Mouse::Left, {104, 144}), click(Mouse::Right, {104, 144}), key(Key::G), key(Key::Digit3), click(Mouse::Left, {40, 128})}
    );
    expect_semantic_history(editor, ground_doc, east, labels);
    EXPECT_EQ(editor.history(0, Mode::Terrain), (HistoryState{1, 1, both_cost}));
    edit(editor, {key(Key::F7)});
    labels[6] = {2};
    expect_semantic_history(editor, both, east, labels);
    // Isolated labels1 and2 select the SAME ID7; history restores meaning, not color.
    edit(editor, {key(Key::T), key(Key::Digit3), click(Mouse::Left, {40, 128})});
    auto alias = both;
    alias.layers[1].cells[0] = "terrain:one";
    labels[6] = {1};
    expect_semantic_history(editor, alias, east, labels);
    EXPECT_EQ(editor.terrain(0, detail).tiles().get(detail, {0, 0}), (Maps::TileId{7}));
    edit(editor, {key(Key::F6)});
    labels[6] = {2};
    expect_semantic_history(editor, both, east, labels);
    edit(editor, {key(Key::F7)});
    labels[6] = {1};
    expect_semantic_history(editor, alias, east, labels);
    edit(editor, {key(Key::F6), key(Key::Digit5), click(Mouse::Left, {72, 144})});
    auto branch = both;
    branch.layers[1].cells[4] = "terrain:three";
    labels[6] = {2};
    labels[10] = {3};
    expect_semantic_history(editor, branch, east, labels);
    EXPECT_EQ(editor.history(0, Mode::Terrain).redo, 0u);
    EXPECT_EQ(editor.document(0).snapshot(), literal);
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, history_limits) {
    const Fixture fixture;
    DemoTilemapEditor seed;
    const auto base = seed.document(0).snapshot();
    auto red = base;
    red.layers[0].cells[4] = "tilemap:red";
    auto brown = base;
    brown.layers[0].cells[4] = "tilemap:brown";
    const auto cost = history_cost(base, red);
    for (const auto limits : {WorldProof::HistoryLimits{0, cost}, WorldProof::HistoryLimits{1, cost - 1}}) {
        DemoTilemapEditor rejected(std::filesystem::path{}, limits);
        edit(rejected, {key(Key::Digit6)});
        const auto* owner = &rejected.document(0);
        EXPECT_THROW(edit(rejected, {click(Mouse::Left, {72, 144})}), WorldProof::Error);
        EXPECT_EQ(&rejected.document(0), owner);
        EXPECT_EQ(rejected.document(0).snapshot(), base);
        EXPECT_EQ(rejected.history(0, Mode::Literal), HistoryState{});
        edit(rejected, {click(Mouse::Right, {72, 144})}); // A no-op needs no history entry, even at zero capacity.
        EXPECT_EQ(rejected.document(0).snapshot(), base);
    }
    DemoTilemapEditor exact(std::filesystem::path{}, {1, cost});
    edit(exact, {key(Key::Digit6), click(Mouse::Left, {72, 144})});
    EXPECT_EQ(exact.document(0).snapshot(), red);
    EXPECT_EQ(exact.history(0, Mode::Literal), (HistoryState{1, 0, cost}));
    edit(exact, {key(Key::F6)});
    EXPECT_EQ(exact.document(0).snapshot(), base);
    const auto redo = exact.history(0, Mode::Literal);
    ASSERT_GT(history_cost(base, brown), cost);
    edit(exact, {key(Key::Digit3)});
    EXPECT_THROW(edit(exact, {click(Mouse::Left, {72, 144})}), WorldProof::Error);
    EXPECT_EQ(exact.document(0).snapshot(), base);
    EXPECT_EQ(exact.history(0, Mode::Literal), redo);
    edit(exact, {key(Key::F7)});
    EXPECT_EQ(exact.document(0).snapshot(), red);
    // Oldest-first entry eviction keeps the new command's undo contract.
    DemoTilemapEditor bounded(std::filesystem::path{}, {2, 1024 * 1024});
    edit(bounded, {key(Key::Digit6), click(Mouse::Left, {72, 144})});
    auto second = red;
    second.layers[1].cells[5] = "tilemap:brown";
    edit(bounded, {key(Key::T), key(Key::Digit3), click(Mouse::Left, {104, 144})});
    EXPECT_EQ(bounded.document(0).snapshot(), second);
    auto third = second;
    third.layers[0].cells[4] = "tilemap:grey";
    edit(bounded, {key(Key::G), key(Key::Digit5), click(Mouse::Left, {72, 144})});
    EXPECT_EQ(bounded.document(0).snapshot(), third);
    EXPECT_EQ(bounded.history(0, Mode::Literal), (HistoryState{2, 0, history_cost(red, second) + history_cost(second, third)}));
    edit(bounded, {key(Key::F6)});
    EXPECT_EQ(bounded.document(0).snapshot(), second);
    edit(bounded, {key(Key::F6)});
    EXPECT_EQ(bounded.document(0).snapshot(), red);
    const auto retained = bounded.history(0, Mode::Literal);
    EXPECT_THROW(edit(bounded, {key(Key::F6)}), WorldProof::Error);
    EXPECT_EQ(bounded.history(0, Mode::Literal), retained);
    EXPECT_EQ(bounded.document(0).snapshot(), red);
    // Exact cumulative byte cap vs one-short causes only the OLDEST command to evict.
    const auto two_bytes = history_cost(base, red) + history_cost(red, second);
    for (const auto bytes : {two_bytes, two_bytes - 1}) {
        DemoTilemapEditor byte_bound(std::filesystem::path{}, {3, bytes});
        edit(byte_bound, {key(Key::Digit6), click(Mouse::Left, {72, 144}), key(Key::T), key(Key::Digit3), click(Mouse::Left, {104, 144})});
        EXPECT_EQ(byte_bound.document(0).snapshot(), second);
        EXPECT_EQ(byte_bound.history(0, Mode::Literal).undo, bytes == two_bytes ? 2u : 1u);
        EXPECT_EQ(byte_bound.history(0, Mode::Literal).bytes, bytes == two_bytes ? two_bytes : history_cost(red, second));
    }
    // The same caps charge semantic KEYS and full metadata, never visual IDs.
    const auto sem = seed.terrain_document(0).snapshot();
    auto one = sem;
    one.layers[0].cells[0] = "terrain:one";
    const auto semantic_cost = history_cost(sem, one);
    for (const auto bytes : {semantic_cost, semantic_cost - 1}) {
        DemoTilemapEditor bounded_semantic(std::filesystem::path{}, {1, bytes});
        edit(bounded_semantic, {key(Key::B)});
        if (bytes == semantic_cost) {
            edit(bounded_semantic, {click(Mouse::Left, {40, 128})});
            EXPECT_EQ(bounded_semantic.terrain_document(0).snapshot(), one);
            EXPECT_EQ(bounded_semantic.history(0, Mode::Terrain), (HistoryState{1, 0, semantic_cost}));
        } else {
            EXPECT_THROW(edit(bounded_semantic, {click(Mouse::Left, {40, 128})}), WorldProof::Error);
            EXPECT_EQ(bounded_semantic.terrain_document(0).snapshot(), sem);
            EXPECT_EQ(bounded_semantic.history(0, Mode::Terrain), HistoryState{});
        }
    }
    for (const auto limits : {WorldProof::HistoryLimits{257, 0}, {1, std::numeric_limits<std::size_t>::max()}})
        EXPECT_THROW(static_cast<void>(DemoTilemapEditor(std::filesystem::path{}, limits)), WorldProof::Error);
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, history_files) {
    const Fixture fixture;
    const DocumentFiles files;
    Documents::save(files.directory / "west.json", custom_document());
    Documents::save(files.directory / "west-terrain.json", semantic_document());
    DemoTilemapEditor editor(files.directory);
    edit(editor, {key(Key::F9), key(Key::Digit6), click(Mouse::Left, {72, 144})});
    auto literal = custom_document();
    literal.layers[0].cells[4] = "tilemap:red";
    const auto literal_history = editor.history(0, Mode::Literal);
    edit(editor, {key(Key::B), key(Key::F9), key(Key::Digit4), click(Mouse::Left, {104, 144})});
    auto semantic = semantic_document();
    semantic.layers[0].cells[5] = "terrain:two";
    const auto semantic_history = editor.history(0, Mode::Terrain);
    edit(editor, {key(Key::Digit2), click(Mouse::Left, {180, 140}), key(Key::V), click(Mouse::Left, {180, 140})});
    const auto east_literal = editor.document(1).snapshot();
    const auto east_semantic = editor.terrain_document(1).snapshot();
    const auto east_lh = editor.history(1, Mode::Literal), east_sh = editor.history(1, Mode::Terrain);
    edit(editor, {key(Key::Digit1), key(Key::F8)});
    EXPECT_EQ(editor.history(0, Mode::Literal), literal_history);
    EXPECT_EQ(Documents::load(files.directory / "west.json"), literal);
    raw_document(files.directory / "west.json", "{");
    EXPECT_THROW(edit(editor, {key(Key::F9)}), Documents::Error);
    EXPECT_EQ(editor.document(0).snapshot(), literal);
    EXPECT_EQ(editor.history(0, Mode::Literal), literal_history);
    Documents::save(files.directory / "west.json", custom_document());
    edit(editor, {key(Key::F9)});
    EXPECT_EQ(editor.document(0).snapshot(), custom_document());
    EXPECT_EQ(editor.history(0, Mode::Literal), HistoryState{});
    EXPECT_EQ(editor.history(0, Mode::Terrain), semantic_history);
    edit(editor, {key(Key::B), key(Key::F8)});
    EXPECT_EQ(editor.history(0, Mode::Terrain), semantic_history);
    EXPECT_EQ(Documents::load(files.directory / "west-terrain.json"), semantic);
    raw_document(files.directory / "west-terrain.json", "{");
    EXPECT_THROW(edit(editor, {key(Key::F9)}), Documents::Error);
    EXPECT_EQ(editor.terrain_document(0).snapshot(), semantic);
    EXPECT_EQ(editor.history(0, Mode::Terrain), semantic_history);
    Documents::save(files.directory / "west-terrain.json", semantic_document());
    edit(editor, {key(Key::F9)});
    EXPECT_EQ(editor.terrain_document(0).snapshot(), semantic_document());
    EXPECT_EQ(editor.history(0, Mode::Terrain), HistoryState{});
    EXPECT_EQ(editor.document(1).snapshot(), east_literal);
    EXPECT_EQ(editor.terrain_document(1).snapshot(), east_semantic);
    EXPECT_EQ(editor.history(1, Mode::Literal), east_lh);
    EXPECT_EQ(editor.history(1, Mode::Terrain), east_sh);
    // Failed SAVE keeps both history and prior runtime edit; file predecessor is independent.
    std::filesystem::rename(files.directory / "west-terrain.json", files.directory / "predecessor.json");
    const auto bytes = document_bytes(files.directory / "predecessor.json");
    std::filesystem::create_symlink(files.directory / "predecessor.json", files.directory / "west-terrain.json");
    edit(editor, {key(Key::T), key(Key::Digit3), click(Mouse::Left, {104, 144})});
    const auto authored = editor.terrain_document(0).snapshot();
    const auto history = editor.history(0, Mode::Terrain);
    EXPECT_THROW(edit(editor, {key(Key::F8)}), Documents::Error);
    EXPECT_EQ(editor.terrain_document(0).snapshot(), authored);
    EXPECT_EQ(editor.history(0, Mode::Terrain), history);
    EXPECT_EQ(document_bytes(files.directory / "predecessor.json"), bytes);
    edit(editor, {key(Key::F6)});
    EXPECT_EQ(editor.terrain_document(0).snapshot(), semantic_document());
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, history_refusals) {
    const Fixture fixture;
    auto palette = DemoTilemapEditor::diagnostic_terrain();
    palette.rule.cases.erase(2);
    DemoTilemapEditor editor(palette);
    // A full board is valid; the later batch needs missing mask2 only after an earlier valid candidate.
    edit(
        editor, {key(Key::B), click(Mouse::Left, {40, 128}), click(Mouse::Left, {72, 128}), click(Mouse::Left, {104, 128}),
                 click(Mouse::Left, {40, 144}), click(Mouse::Left, {72, 144}), click(Mouse::Left, {104, 144})}
    );
    const auto before = editor.terrain_document(0).snapshot();
    const auto history = editor.history(0, Mode::Terrain);
    const auto visuals = editor_terrain_tiles(editor);
    EXPECT_THROW(edit(editor, {click(Mouse::Right, {40, 128}), click(Mouse::Right, {104, 144})}), bad_request);
    EXPECT_EQ(editor.terrain_document(0).snapshot(), before);
    EXPECT_EQ(editor.history(0, Mode::Terrain), history);
    EXPECT_EQ(editor_terrain_tiles(editor), visuals);
    edit(editor, {key(Key::F6)});
    const auto undone = editor.terrain_document(0).snapshot();
    const auto redo = editor.history(0, Mode::Terrain);
    EXPECT_THROW(edit(editor, {click(Mouse::Left, {40, 128}), click(Mouse::Left, {72, 128})}), bad_request);
    EXPECT_EQ(editor.terrain_document(0).snapshot(), undone);
    EXPECT_EQ(editor.history(0, Mode::Terrain), redo);
    edit(editor, {key(Key::F7)});
    EXPECT_EQ(editor.terrain_document(0).snapshot(), before);
    // Deliberate negative alias write violates read-only facade; witness refusal preserves both stacks/content.
    DemoTilemapEditor literal;
    edit(literal, {key(Key::Digit6), click(Mouse::Left, {72, 144})});
    const auto valid = literal.document(0).snapshot();
    const auto lh = literal.history(0, Mode::Literal);
    const_cast<Maps::Map&>(literal.map(0)).set(ground, {0, 0}, {8});
    const auto altered = literal.document(0).snapshot();
    EXPECT_THROW(edit(literal, {key(Key::F6)}), WorldProof::Error);
    EXPECT_EQ(literal.document(0).snapshot(), altered);
    EXPECT_EQ(literal.history(0, Mode::Literal), lh);
    const_cast<Maps::Map&>(literal.map(0)).set(ground, {0, 0}, {7});
    edit(literal, {key(Key::F6)});
    const auto prior = literal.document(0).snapshot();
    const auto rh = literal.history(0, Mode::Literal);
    const_cast<Maps::Map&>(literal.map(0)).set(ground, {0, 0}, {999});
    EXPECT_THROW(edit(literal, {key(Key::F7)}), WorldProof::Error);
    EXPECT_EQ(literal.map(0).get(ground, {0, 0}), (Maps::TileId{999}));
    EXPECT_EQ(literal.history(0, Mode::Literal), rh);
    const_cast<Maps::Map&>(literal.map(0)).set(ground, {0, 0}, {7});
    EXPECT_EQ(literal.document(0).snapshot(), prior);
    edit(literal, {key(Key::F7)});
    EXPECT_EQ(literal.document(0).snapshot(), valid);
    // Real demo update: earlier paint commits; empty REDO rejects and drops following map switch/save.
    const DocumentFiles files;
    DemoTilemaps demo(files.directory);
    const std::array tick{key(Key::Digit6), click(Mouse::Left, {72, 144}), key(Key::F7), key(Key::Digit2), key(Key::F8)};
    testing::internal::CaptureStderr();
    demo.update(tick, editor_sizes);
    const auto diagnostic = testing::internal::GetCapturedStderr();
    EXPECT_NE(diagnostic.find("document history rejected"), std::string::npos);
    EXPECT_NE(diagnostic.find("redo history is empty"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(files.directory / "east.json"));
    const std::array save{key(Key::F8)};
    demo.update(save, editor_sizes);
    EXPECT_EQ(Documents::load(files.directory / "west.json").layers[0].cells[4], "tilemap:red");
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, history_filters) {
    DemoTilemapEditor editor;
    const auto base = editor.document(0).snapshot();
    edit(editor, {key(Key::Digit6), click(Mouse::Left, {72, 144})});
    const auto painted = editor.document(0).snapshot();
    const auto history = editor.history(0, Mode::Literal);
    auto denied = key(Key::F6);
    denied.to_gameplay = false;
    const std::array ignored{key(Key::F6, Events::ButtonPhase::Release), key(Key::F7, Events::ButtonPhase::Repeat), denied};
    editor.consume(ignored, editor_sizes);
    EXPECT_EQ(editor.document(0).snapshot(), painted);
    EXPECT_EQ(editor.history(0, Mode::Literal), history);
    const std::array undo{key(Key::F6)};
    editor.consume(undo, {{0, 200}, {800, 600}});
    EXPECT_EQ(editor.document(0).snapshot(), painted);
    EXPECT_EQ(editor.history(0, Mode::Literal), history);
    edit(editor, {key(Key::D), key(Key::T), key(Key::Digit4), key(Key::F6)});
    EXPECT_EQ(editor.document(0).snapshot(), base);
    EXPECT_EQ(editor.selected_layer(), detail);
    EXPECT_EQ(editor.brush(), (Maps::TileId{8}));
    EXPECT_DOUBLE_EQ(editor.pan().x, 32);
    const auto redo = editor.history(0, Mode::Literal);
    edit(editor, {key(Key::G), key(Key::R), key(Key::Digit3), click(Mouse::Left, {40, 128})}); // Existing brown is no-op.
    EXPECT_EQ(editor.history(0, Mode::Literal), redo);
    EXPECT_EQ(editor.document(0).snapshot(), base);
    {
        std::vector<Events::Record> ephemeral{key(Key::F7)};
        editor.consume(ephemeral, editor_sizes);
        ephemeral.clear();
    }
    EXPECT_EQ(editor.document(0).snapshot(), painted);
    EXPECT_EQ(editor.history(0, Mode::Literal), history);
    edit(editor, {key(Key::F6), key(Key::F7)});
    EXPECT_EQ(editor.document(0).snapshot(), painted);
}

TEST(tilemap_consumer, history_frames) {
    const auto calls = std::make_shared<Calls>();
    const DocumentFiles files;
    Render::Frame original, painted, undone, redone;
    std::vector<Render::DrawPacket2D> a, b, c, d;
    std::weak_ptr<Image> image;
    std::weak_ptr<Geometry> geometry;
    Documents::Snapshot held;
    {
        const Fixture fixture(calls);
        image = fixture.image;
        geometry = fixture.geometry;
        Documents::save(files.directory / "west-terrain.json", semantic_document());
        DemoTilemapEditor editor(files.directory);
        edit(editor, {key(Key::B), key(Key::F9)});
        held = editor.terrain_document(0).snapshot();
        prepare_editor(original, editor, fixture);
        a = original.passes()[0].draws;
        edit(editor, {key(Key::Digit6), click(Mouse::Left, {40, 144}), key(Key::T), key(Key::Digit3), click(Mouse::Left, {40, 128})});
        auto expected = semantic_document();
        expected.layers[0].cells[3] = "terrain:four";
        expected.layers[1].cells[0] = "terrain:one";
        EXPECT_EQ(editor.terrain_document(0).snapshot(), expected);
        auto labels = semantic_expected();
        labels[3] = {4};
        labels[6] = {1};
        EXPECT_EQ(editor_terrain_tiles(editor), diagnostic_tiles(labels));
        prepare_editor(painted, editor, fixture);
        b = painted.passes()[0].draws;
        edit(editor, {key(Key::F6), key(Key::F6)});
        EXPECT_EQ(editor.terrain_document(0).snapshot(), held);
        prepare_editor(undone, editor, fixture);
        c = undone.passes()[0].draws;
        expect_packets(undone, a);
        edit(editor, {key(Key::F7), key(Key::F7)});
        EXPECT_EQ(editor.terrain_document(0).snapshot(), expected);
        prepare_editor(redone, editor, fixture);
        d = redone.passes()[0].draws;
        expect_packets(redone, b);
        EXPECT_EQ(held, semantic_document());
        expect_packets(original, a);
        expect_packets(painted, b);
        calls->expect_cpu();
    }
    expect_packets(original, a);
    expect_packets(painted, b);
    expect_packets(undone, c);
    expect_packets(redone, d);
    EXPECT_EQ(held, semantic_document());
    EXPECT_FALSE(image.expired());
    EXPECT_FALSE(geometry.expired());
    original.recycle();
    painted.recycle();
    undone.recycle();
    redone.recycle();
    a.clear();
    b.clear();
    c.clear();
    EXPECT_FALSE(image.expired());
    d.clear();
    EXPECT_TRUE(image.expired());
    EXPECT_TRUE(geometry.expired());
    calls->expect_cpu();
}

namespace {
    namespace Variable = VariableDocuments;
    // Independent inverse of the documented app viewport transform. Explicitly
    // unequal logical/framebuffer scales, fixed origins/cell sizes, no live input.
    Events::Pointer domain_pointer(const DemoTilemapEditor& editor, double x, double y) {
        const double ox = editor.selected_map() == 0 ? 48 : 328;
        const double oy = editor.selected_map() == 0 ? 240 : 208;
        return {(ox + x - editor.pan().x) * editor.zoom() / 2, (600 - (oy - y - editor.pan().y) * editor.zoom()) / 3};
    }
    Events::Pointer domain_cell(const DemoTilemapEditor& editor, std::uint32_t x, std::uint32_t y) {
        return domain_pointer(editor, (double(x) + 0.5) * 64, (double(y) + 0.5) * 48);
    }
    void domain_view(DemoTilemapEditor& editor) {
        edit(editor, {});
        ASSERT_TRUE(editor.set_view({0, -3072}, 0.125)); // Entire maximum footprint, exact binary zoom.
    }
    void expect_domain(const DemoTilemapEditor& editor, std::size_t index, const Documents::Snapshot& source, bool semantic) {
        ASSERT_EQ(semantic ? editor.terrain_document(index).snapshot() : editor.document(index).snapshot(), source);
        const Maps::Extent extent{source.width, source.height};
        if (!semantic) {
            ASSERT_EQ(editor.map(index).extent(), extent);
            ASSERT_EQ(editor.map(index).layer_count(), 2u);
        }
        for (std::int64_t layer = 0; layer < 2; ++layer) {
            const Maps::LayerIndex selected{layer};
            if (semantic) {
                ASSERT_EQ(editor.terrain(index, selected).tiles().extent(), extent);
                ASSERT_EQ(editor.terrain(index, selected).tiles().layer_count(), 2u);
            }
            for (std::uint32_t y = 0; y < source.height; ++y)
                for (std::uint32_t x = 0; x < source.width; ++x) {
                    const Maps::Point point{x, y};
                    const auto& key = source.layers[static_cast<std::size_t>(layer)].cells[std::size_t(y) * source.width + x];
                    if (semantic) {
                        EXPECT_EQ(editor.terrain(index, selected).label(point).value, Variable::meaning(key, true));
                        EXPECT_EQ(
                            editor.terrain(index, selected).tiles().get(selected, point).value,
                            Variable::visual(source, static_cast<std::size_t>(layer), x, y)
                        );
                    } else {
                        EXPECT_EQ(editor.map(index).get(selected, point).value, Variable::numeric(key, false));
                    }
                }
        }
    }
}

TEST(tilemap_consumer, variable_literal_documents) {
    const Fixture fixture;
    const DocumentFiles files;
    DemoTilemapEditor editor(files.directory);
    domain_view(editor);
    const auto terrain = editor.terrain_document(0).snapshot();
    const auto east = Variable::source(1, 7, false, true);
    Documents::save(files.directory / "east.json", east);
    edit(editor, {key(Key::Digit2), key(Key::F9), key(Key::Digit1)});
    for (const auto shape : Variable::shapes) {
        auto expected = Variable::source(shape[0], shape[1], false);
        Documents::save(files.directory / "west.json", expected);
        edit(editor, {key(Key::F9), key(Key::T), key(Key::Digit6)});
        expect_domain(editor, 0, expected, false);
        EXPECT_EQ(editor.history(0, Mode::Literal), HistoryState{});
        const auto last = domain_cell(editor, shape[0] - 1, shape[1] - 1);
        // Erase first so the following paint is a changing command for every pattern.
        edit(editor, {click(Mouse::Right, last)});
        auto erased = expected;
        erased.layers[1].cells.back().reset();
        expect_domain(editor, 0, erased, false);
        edit(editor, {click(Mouse::Left, last)});
        auto painted = erased;
        painted.layers[1].cells.back() = "tilemap:red";
        expect_domain(editor, 0, painted, false);
        edit(editor, {key(Key::F6)});
        expect_domain(editor, 0, erased, false);
        edit(editor, {key(Key::F7), key(Key::F8)});
        EXPECT_EQ(Documents::load(files.directory / "west.json"), painted);
        const auto history = editor.history(0, Mode::Literal);
        ASSERT_GT(history.undo, 0u);
        edit(editor, {click(Mouse::Right, last), key(Key::F9)});
        expect_domain(editor, 0, painted, false);
        EXPECT_EQ(editor.history(0, Mode::Literal), HistoryState{});
        expect_domain(editor, 1, east, false);
        EXPECT_EQ(editor.terrain_document(0).snapshot(), terrain);
    }
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, variable_semantic_documents) {
    const Fixture fixture;
    const DocumentFiles files;
    DemoTilemapEditor editor(files.directory);
    domain_view(editor);
    const auto literal = editor.document(0).snapshot();
    auto east = Variable::source(7, 1, true, true);
    Documents::save(files.directory / "east-terrain.json", east);
    edit(editor, {key(Key::B), key(Key::Digit2), key(Key::F9), key(Key::Digit1)});
    for (const auto shape : Variable::shapes) {
        auto expected = Variable::source(shape[0], shape[1], true);
        Documents::save(files.directory / "west-terrain.json", expected);
        edit(editor, {key(Key::F9), key(Key::T), key(Key::Digit4)});
        expect_domain(editor, 0, expected, true);
        const auto last = domain_cell(editor, shape[0] - 1, shape[1] - 1);
        edit(editor, {click(Mouse::Right, last)});
        expected.layers[1].cells.back().reset();
        expect_domain(editor, 0, expected, true);
        edit(editor, {click(Mouse::Left, last)});
        auto painted = expected;
        painted.layers[1].cells.back() = "terrain:two";
        expect_domain(editor, 0, painted, true);
        edit(editor, {key(Key::F6)});
        expect_domain(editor, 0, expected, true);
        edit(editor, {key(Key::F7), key(Key::F8)});
        EXPECT_EQ(Documents::load(files.directory / "west-terrain.json"), painted);
        edit(editor, {click(Mouse::Right, last), key(Key::F9)});
        expect_domain(editor, 0, painted, true);
        EXPECT_EQ(editor.history(0, Mode::Terrain), HistoryState{});
        expect_domain(editor, 1, east, true);
        EXPECT_EQ(editor.document(0).snapshot(), literal);
    }
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, variable_picking) {
    const DocumentFiles files;
    const auto literal = Variable::source(7, 5, false);
    const auto semantic = Variable::source(1, 7, true);
    Documents::save(files.directory / "west.json", literal);
    Documents::save(files.directory / "west-terrain.json", semantic);
    DemoTilemapEditor editor(files.directory);
    domain_view(editor);
    edit(editor, {key(Key::F9)});
    EXPECT_EQ(editor.pick(domain_cell(editor, 6, 4)), (Maps::Point{6, 4}));
    EXPECT_EQ(editor.pick(domain_pointer(editor, 64, 48)), (Maps::Point{1, 1}));
    EXPECT_FALSE(editor.pick(domain_pointer(editor, 7 * 64, 5 * 48)));
    EXPECT_FALSE(editor.pick(domain_pointer(editor, 7 * 64, 0)));
    EXPECT_FALSE(editor.pick(domain_pointer(editor, 0, 5 * 48)));
    edit(editor, {key(Key::B), key(Key::F9), key(Key::Digit4)});
    EXPECT_EQ(editor.pick(domain_cell(editor, 0, 6)), (Maps::Point{0, 6})); // Outside literal height, INSIDE active terrain.
    EXPECT_FALSE(editor.pick(domain_cell(editor, 1, 0)));                   // Inside literal width, OUTSIDE active terrain.
    auto painted = semantic;
    painted.layers[0].cells[6] = "terrain:two";
    edit(editor, {click(Mouse::Left, domain_cell(editor, 0, 6))});
    expect_domain(editor, 0, painted, true);
    const auto state = editor.history(0, Mode::Terrain);
    edit(editor, {click(Mouse::Left, domain_cell(editor, 1, 0)), click(Mouse::Right, domain_pointer(editor, 0, 7 * 48))});
    EXPECT_EQ(editor.history(0, Mode::Terrain), state);
    expect_domain(editor, 0, painted, true);
    // Nonuniform framebuffer scales, nonzero pan and computed internal edge ownership.
    ASSERT_TRUE(editor.set_view({16, -129}, 1));
    const auto edge = domain_pointer(editor, 0, 48);
    EXPECT_EQ(editor.pick(edge), (Maps::Point{0, 1}));
    EXPECT_EQ(editor.pick({edge.x, std::nextafter(edge.y, 0.0)}), (Maps::Point{0, 0}));
    EXPECT_EQ(editor.pick({edge.x, std::nextafter(edge.y, 200.0)}), (Maps::Point{0, 1}));
    EXPECT_FALSE(editor.pick(domain_pointer(editor, -1, 0)));
    edit(editor, {key(Key::V)});
    expect_domain(editor, 0, literal, false);
}

TEST(tilemap_consumer, variable_limits_and_late_load) {
    const Fixture fixture;
    const DocumentFiles files;
    const auto base = Variable::source(7, 5, false);
    Documents::save(files.directory / "west.json", base);
    DemoTilemapEditor editor(files.directory);
    domain_view(editor);
    edit(editor, {key(Key::F9), key(Key::Digit6), click(Mouse::Left, domain_cell(editor, 0, 0))});
    const auto before = editor.document(0).snapshot();
    const auto history = editor.history(0, Mode::Literal);
    const auto east = editor.document(1).snapshot();
    for (int failure = 0; failure < 11; ++failure) {
        auto invalid = Variable::source(7, 5, false);
        if (failure == 0 || failure == 1)
            invalid = Variable::source(failure == 0 ? 65 : 1, failure == 1 ? 65 : 1, false);
        if (failure == 2) {
            invalid.layers.pop_back();
            invalid.spawns.clear();
        }
        if (failure == 3)
            invalid.layers.push_back({"third", invalid.layers[0].cells});
        if (failure == 4)
            invalid.layers[1].cells.back() = "tilemap:unknown";
        if (failure == 5)
            invalid.spawns.back().x = invalid.width;
        if (failure == 6)
            invalid.width = 0;
        if (failure == 7)
            invalid.spawns.resize(257, invalid.spawns.front());
        if (failure == 8)
            invalid.layers[1].cells.pop_back();
        // encode deliberately refuses invalid storage shapes; write raw encoded-valid
        // fixtures for editor-only refusals, otherwise inject an independently malformed JSON.
        if (failure <= 4)
            Documents::save(files.directory / "west.json", invalid);
        else {
            auto text = Documents::encode(base);
            const auto replace = [&](const std::string& from, const std::string& to) {
                const auto at = text.find(from);
                ASSERT_NE(at, std::string::npos);
                text.replace(at, from.size(), to);
            };
            if (failure == 5)
                replace("\"x\": 6", "\"x\": 7");
            if (failure == 6)
                replace("\"width\": 7", "\"width\": 0");
            if (failure == 7) {
                const auto at = text.find("\"spawns\": [");
                ASSERT_NE(at, std::string::npos);
                text.resize(at);
                text += "\"spawns\": [";
                for (int i = 0; i < 257; ++i) {
                    if (i)
                        text += ',';
                    text += "{\"name\":\"spawn" + std::to_string(i) + "\",\"layer\":\"floor\",\"x\":0,\"y\":0}";
                }
                text += "]}";
            }
            if (failure == 8)
                replace("\"height\": 5", "\"height\": 4");
            if (failure == 9)
                replace("\"height\": 5", "\"height\": 0");
            if (failure == 10)
                replace("tilemap:brown", "tilemap:" + std::string(122, 'a'));
            raw_document(files.directory / "west.json", text);
        }
        EXPECT_THROW(edit(editor, {key(Key::F9)}), std::exception);
        EXPECT_EQ(editor.document(0).snapshot(), before);
        EXPECT_EQ(editor.history(0, Mode::Literal), history);
        EXPECT_EQ(editor.document(1).snapshot(), east);
    }
    auto palette = DemoTilemapEditor::diagnostic_terrain();
    palette.rule.cases.erase(2);
    DemoTilemapEditor late(palette, {32, 160, 1024}, files.directory);
    domain_view(late);
    edit(late, {key(Key::B), click(Mouse::Left, domain_cell(late, 0, 0))}); // Isolated mask0 valid, nonempty history.
    const auto old = late.terrain_document(0).snapshot();
    const auto old_history = late.history(0, Mode::Terrain);
    auto replacement = Variable::source(7, 5, true);
    for (auto& layer : replacement.layers)
        std::fill(layer.cells.begin(), layer.cells.end(), "terrain:one");
    std::fill(replacement.layers[1].cells.begin(), replacement.layers[1].cells.end(), std::nullopt);
    replacement.layers[1].cells[0] = replacement.layers[1].cells[1] = "terrain:two"; // First selected cell requires missing mask2.
    const WorldProof::KeyTable table(semantic_keys);
    const auto first = WorldProof::TerrainOwner::from(replacement, table, ground, semantic_labels, palette.rule, palette.target_to_id);
    ASSERT_EQ(first->terrain().tiles().extent(), (Maps::Extent{7, 5})); // Larger FIRST owner proven valid.
    Documents::save(files.directory / "west-terrain.json", replacement);
    Render::Frame retained;
    prepare_editor(retained, late, fixture);
    const auto packets = retained.passes()[0].draws;
    EXPECT_THROW(edit(late, {key(Key::F9)}), bad_request);
    EXPECT_EQ(late.terrain_document(0).snapshot(), old);
    EXPECT_EQ(late.history(0, Mode::Terrain), old_history);
    expect_packets(retained, packets);
    // Real update path: bad admission drops later SAVE; later tick may load the valid document.
    DemoTilemaps demo(files.directory);
    Documents::save(files.directory / "west.json", Variable::source(65, 1, false));
    const auto bytes = document_bytes(files.directory / "west.json");
    testing::internal::CaptureStderr();
    const std::array bad_tick{key(Key::F9), key(Key::F8)};
    demo.update(bad_tick, editor_sizes);
    const auto diagnostic = testing::internal::GetCapturedStderr();
    EXPECT_NE(diagnostic.find("axes 1..64"), std::string::npos);
    EXPECT_EQ(document_bytes(files.directory / "west.json"), bytes);
    Documents::save(files.directory / "west.json", base);
    const std::array good_tick{key(Key::F9), key(Key::F8)};
    demo.update(good_tick, editor_sizes);
    EXPECT_EQ(Documents::load(files.directory / "west.json"), base);
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, variable_stroke_budgets) {
    const DocumentFiles files;
    auto base = Variable::source(64, 64, true);
    for (auto& layer : base.layers)
        std::fill(layer.cells.begin(), layer.cells.end(), "terrain:one");
    Documents::save(files.directory / "west-terrain.json", base);
    auto expected = base;
    for (std::uint32_t y = 0; y < 4; ++y)
        for (std::uint32_t x = 0; x < 8; ++x)
            expected.layers[0].cells[std::size_t(2 + 8 * y) * 64 + 2 + 8 * x] = "terrain:two";
    // 32 interior, nonoverlapping five-cell crosses: 32 + 5*32 + 5*160 = 992 work.
    for (const auto limits : {Maps::TerrainBatchLimits{32, 160, 1024}, {32, 160, 992}, {31, 160, 992}, {32, 159, 992}, {32, 160, 991}}) {
        DemoTilemapEditor editor(DemoTilemapEditor::diagnostic_terrain(), limits, files.directory);
        domain_view(editor);
        edit(editor, {key(Key::B), key(Key::F9), key(Key::Digit4)});
        std::vector<Events::Record> stroke;
        for (std::uint32_t y = 0; y < 4; ++y)
            for (std::uint32_t x = 0; x < 8; ++x)
                stroke.push_back(click(Mouse::Left, domain_cell(editor, 2 + 8 * x, 2 + 8 * y)));
        const bool admitted = limits.requests == 32 && limits.affected_cells == 160 && limits.work >= 992;
        if (admitted) {
            editor.consume(stroke, editor_sizes);
            expect_domain(editor, 0, expected, true);
            ASSERT_EQ(editor.history(0, Mode::Terrain).undo, 1u);
            edit(editor, {key(Key::F6)});
            expect_domain(editor, 0, base, true);
            edit(editor, {key(Key::F7)});
            expect_domain(editor, 0, expected, true);
        } else {
            EXPECT_THROW(editor.consume(stroke, editor_sizes), bad_request);
            expect_domain(editor, 0, base, true);
            EXPECT_EQ(editor.history(0, Mode::Terrain), HistoryState{});
        }
    }
    const auto cost = history_cost(base, expected);
    ASSERT_LE(cost, WorldProof::HistoryLimits{}.bytes);
    for (const auto bytes : {cost, cost - 1}) {
        DemoTilemapEditor editor(DemoTilemapEditor::diagnostic_terrain(), {32, 160, 1024}, files.directory, {1, bytes});
        domain_view(editor);
        edit(editor, {key(Key::B), key(Key::F9), key(Key::Digit4)});
        std::vector<Events::Record> stroke;
        for (std::uint32_t y = 0; y < 4; ++y)
            for (std::uint32_t x = 0; x < 8; ++x)
                stroke.push_back(click(Mouse::Left, domain_cell(editor, 2 + 8 * x, 2 + 8 * y)));
        if (bytes == cost) {
            editor.consume(stroke, editor_sizes);
            EXPECT_EQ(editor.history(0, Mode::Terrain), (HistoryState{1, 0, cost}));
            expect_domain(editor, 0, expected, true);
        } else {
            EXPECT_THROW(editor.consume(stroke, editor_sizes), WorldProof::Error);
            expect_domain(editor, 0, base, true);
            EXPECT_EQ(editor.history(0, Mode::Terrain), HistoryState{});
        }
    }
}

TEST(tilemap_consumer, variable_frames_and_clipping) {
    const auto calls = std::make_shared<Calls>();
    Render::Frame old;
    std::vector<Render::DrawPacket2D> packets;
    std::weak_ptr<Image> image;
    const DocumentFiles files;
    {
        const Fixture fixture(calls);
        image = fixture.image;
        DemoTilemapEditor editor(files.directory);
        auto west = Variable::source(7, 5, false);
        auto east = Variable::source(1, 7, false, true);
        Documents::save(files.directory / "west.json", west);
        Documents::save(files.directory / "east.json", east);
        edit(editor, {key(Key::F9), key(Key::Digit2), key(Key::F9), key(Key::Digit1)});
        // World viewport [112,912] x [-48,552]; independent rectangle oracle for each cell.
        ASSERT_TRUE(editor.set_view({112, -48}, 1));
        prepare_editor(old, editor, fixture);
        std::vector<ExpectedDraw> expected;
        for (std::size_t instance = 0; instance < 2; ++instance) {
            const auto& source = instance == 0 ? west : east;
            const double ox = instance == 0 ? 48 : 328, oy = instance == 0 ? 240 : 208;
            for (const auto& layer : source.layers)
                for (std::uint32_t y = 0; y < source.height; ++y)
                    for (std::uint32_t x = 0; x < source.width; ++x) {
                        const auto id = Variable::numeric(layer.cells[std::size_t(y) * source.width + x], false);
                        const auto left = ox + x * 64, top = oy - y * 48;
                        if (id && left < 912 && left + 64 > 112 && top > -48 && top - 48 < 552)
                            expected.push_back({(id - 7) * 4, static_cast<float>(left + 16), static_cast<float>(top - 36)});
                    }
        }
        expect_draws(old, expected);
        packets = old.passes()[0].draws;
        const auto held = editor.document(0).snapshot();
        auto large = Variable::source(64, 64, false);
        Documents::save(files.directory / "west.json", large);
        edit(editor, {key(Key::F9)});
        domain_view(editor);
        edit(editor, {key(Key::Digit6), click(Mouse::Left, domain_cell(editor, 63, 63)), key(Key::F6), key(Key::F7)});
        expect_domain(editor, 1, east, false);
        EXPECT_EQ(held, west);
        expect_packets(old, packets);
        calls->expect_cpu();
    }
    expect_packets(old, packets);
    EXPECT_FALSE(image.expired());
    old.recycle();
    packets.clear();
    EXPECT_TRUE(image.expired());
    calls->expect_cpu();
}

namespace {
    namespace Generated = GeneratedExpectations;
    namespace Patches = CE::Maps::CellPatches;
    using Replace = WorldProof::GeneratedReplace;
    void expect_attachment(const WorldProof::GeneratedDocument& actual, const WorldProof::GeneratedDocument& expected) {
        EXPECT_EQ(actual.recipe, expected.recipe);
        EXPECT_EQ(actual.effective, expected.effective);
        EXPECT_EQ(actual.limits, expected.limits);
        EXPECT_EQ(actual.base, expected.base);
        EXPECT_EQ(actual.overlay, expected.overlay);
        EXPECT_EQ(actual.bytes, expected.bytes);
    }
    WorldProof::CaveRecipe small_recipe() {
        auto result = Generated::recipe();
        const std::array<ProcGen::Index, 2> size{5, 4};
        result.cave.domain = *ProcGen::Domain::checked(size);
        result.cave.requested.solid_fill = 10000;
        result.cave.requested.steps = 0;
        result.adapter.spawns = {{"first", "floor", 0, 0}, {"last", "overlay", 4, 3}};
        return result;
    }
    Documents::Snapshot generated_final() {
        auto final = Generated::base();
        final.layers[0].cells[2 + 2 * 31] = "terrain:four";
        final.layers[0].cells[3 + 2 * 31].reset();
        final.layers[1].cells[1 + 31] = final.layers[1].cells[2 + 31] = "terrain:one";
        return final;
    }
    void paint_generated(DemoTilemapEditor& editor) {
        edit(
            editor, {key(Key::G), key(Key::Digit6), click(Mouse::Left, domain_cell(editor, 2, 2)),
                     click(Mouse::Left, domain_cell(editor, 2, 2)), click(Mouse::Right, domain_cell(editor, 3, 2))}
        );
        edit(
            editor,
            {key(Key::T), key(Key::Digit3), click(Mouse::Left, domain_cell(editor, 1, 1)), click(Mouse::Left, domain_cell(editor, 2, 1))}
        );
    }
}

TEST(tilemap_consumer, generated_replay) {
    const Fixture fixture;
    DemoTilemapEditor editor;
    domain_view(editor);
    EXPECT_THROW(editor.install_generated(Generated::recipe(), Replace::DiscardCurrent), WorldProof::Error);
    edit(editor, {key(Key::B)});
    EXPECT_THROW(editor.install_generated(Generated::recipe(), Replace::PreserveEdits), WorldProof::Error);
    const auto literal = editor.document(0).snapshot();
    for (std::size_t choice = 0; choice < 3; ++choice) {
        auto recipe = Generated::recipe(choice);
        editor.install_generated(recipe, Replace::DiscardCurrent);
        const auto expected = Generated::base(choice);
        expect_domain(editor, 0, expected, true);
        const auto state = *editor.generated_state(0);
        EXPECT_EQ(state.base, expected);
        EXPECT_EQ(state.effective, recipe.cave.requested);
        EXPECT_EQ(state.overlay.witness, Patches::capture(expected));
        EXPECT_TRUE(state.overlay.edits.empty());
        EXPECT_EQ(Patches::apply(expected, state.overlay), expected);
        const auto* owner = &editor.terrain_document(0);
        editor.regenerate_selected();
        EXPECT_EQ(&editor.terrain_document(0), owner);
        expect_attachment(*editor.generated_state(0), state);
        auto larger_budget = recipe;
        larger_budget.cave.budgets.work += 1;
        editor.install_generated(larger_budget, Replace::PreserveEdits);
        EXPECT_EQ(&editor.terrain_document(0), owner);
        EXPECT_EQ(editor.generated_state(0)->recipe, larger_budget);
        expect_domain(editor, 0, expected, true);
        EXPECT_EQ(editor.document(0).snapshot(), literal);
        EXPECT_EQ(editor.history(0, Mode::Terrain), HistoryState{});
        // The live attachment owns every recipe/base/witness byte.
        recipe.adapter.id = "terrain:destroyed-input";
        recipe.adapter.spawns.clear();
        auto copy = *editor.generated_state(0);
        copy.base.layers.clear();
        copy.recipe.adapter.spawns.clear();
        copy.overlay.edits.push_back({});
        expect_domain(editor, 0, expected, true);
    }
    EXPECT_NE(Generated::base(0), Generated::base(1)); // Seed-only change, same stage/options.
    EXPECT_NE(Generated::base(0), Generated::base(2)); // Declared different stage/configuration.
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, generated_edits) {
    const Fixture fixture;
    DemoTilemapEditor editor;
    domain_view(editor);
    edit(editor, {key(Key::B)});
    editor.install_generated(Generated::recipe(), Replace::DiscardCurrent);
    const auto literal = editor.document(0).snapshot();
    edit(editor, {key(Key::Digit2)});
    auto east_recipe = Generated::recipe(1);
    east_recipe.adapter.id = "terrain:generated-east";
    editor.install_generated(east_recipe, Replace::DiscardCurrent);
    const auto east = *editor.generated_state(1);
    const auto east_content = editor.terrain_document(1).snapshot();
    edit(editor, {key(Key::Digit1)});
    paint_generated(editor);
    const auto final = generated_final();
    expect_domain(editor, 0, final, true);
    auto state = *editor.generated_state(0);
    EXPECT_EQ(state.overlay, Patches::diff(Generated::base(), final));
    ASSERT_EQ(state.overlay.edits.size(), 4u);
    EXPECT_EQ(state.overlay.edits[0], (Patches::Edit{"floor", 2, 2, "terrain:four"}));
    EXPECT_EQ(state.overlay.edits[1], (Patches::Edit{"floor", 3, 2, std::nullopt}));
    EXPECT_EQ(state.overlay.edits[2], (Patches::Edit{"overlay", 1, 1, "terrain:one"}));
    EXPECT_EQ(state.overlay.edits[3], (Patches::Edit{"overlay", 2, 1, "terrain:one"}));
    ASSERT_EQ(editor.history(0, Mode::Terrain).undo, 2u);
    Render::Frame old;
    prepare_editor(old, editor, fixture);
    const auto packets = old.passes()[0].draws;
    edit(editor, {key(Key::F6)});
    auto ground = final;
    ground.layers[1].cells = Generated::base().layers[1].cells;
    expect_domain(editor, 0, ground, true);
    EXPECT_EQ(editor.generated_state(0)->overlay, Patches::diff(Generated::base(), ground));
    const auto* held = &editor.terrain_document(0);
    const auto undone = *editor.generated_state(0);
    editor.regenerate_selected();
    EXPECT_EQ(&editor.terrain_document(0), held);
    expect_attachment(*editor.generated_state(0), undone);
    edit(editor, {key(Key::F7)});
    expect_domain(editor, 0, final, true);
    expect_attachment(*editor.generated_state(0), state);
    editor.regenerate_selected();
    expect_packets(old, packets);
    expect_attachment(*editor.generated_state(1), east);
    EXPECT_EQ(editor.terrain_document(1).snapshot(), east_content);
    EXPECT_EQ(editor.document(0).snapshot(), literal);
    edit(editor, {key(Key::F6), key(Key::Digit4), click(Mouse::Left, domain_cell(editor, 1, 1))});
    EXPECT_EQ(editor.history(0, Mode::Terrain).redo, 0u);
    auto alias = ground;
    alias.layers[1].cells[32] = "terrain:two";
    expect_domain(editor, 0, alias, true);
    EXPECT_EQ(editor.terrain(0, detail).tiles().get(detail, {1, 1}), (Maps::TileId{7})); // Same isolated color as label1, distinct key2.
    EXPECT_EQ(editor.generated_state(0)->overlay, Patches::diff(Generated::base(), alias));
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, generated_limits) {
    const auto recipe = small_recipe();
    auto limits = WorldProof::GeneratedLimits{};
    limits.edits = 2;
    const auto initial = WorldProof::generate_document(recipe, limits);
    auto final = initial.base;
    final.layers[0].cells[0] = final.layers[0].cells[1] = "terrain:one";
    const auto changed = WorldProof::stage_generated_edits(initial, final);
    for (const auto boundary : {0, 1, 2}) {
        for (const bool one_short : {false, true}) {
            auto cap = limits;
            if (boundary == 0)
                cap.witness_bytes = initial.bytes.witness - (one_short ? 1u : 0u);
            if (boundary == 1)
                cap.retained_bytes = initial.bytes.retained - (one_short ? 1u : 0u);
            if (boundary == 2)
                cap.work = initial.bytes.reserved_work - (one_short ? 1u : 0u);
            if (one_short)
                EXPECT_THROW(static_cast<void>(WorldProof::generate_document(recipe, cap)), WorldProof::Error);
            else
                EXPECT_EQ(WorldProof::generate_document(recipe, cap).base, initial.base);
        }
    }
    const auto req = *ProcGen::cave_requirements(recipe.cave.domain, recipe.cave.requested);
    for (const auto boundary : {0, 1, 2}) {
        auto exact = recipe;
        exact.cave.budgets = {req.cells, req.storage_bytes, req.work_reservation};
        EXPECT_EQ(WorldProof::generate_document(exact, limits).base, initial.base);
        if (boundary == 0)
            --exact.cave.budgets.cells;
        if (boundary == 1)
            --exact.cave.budgets.storage_bytes;
        if (boundary == 2)
            --exact.cave.budgets.work;
        EXPECT_THROW(static_cast<void>(WorldProof::generate_document(exact, limits)), WorldProof::Error);
    }
    for (const bool one_short : {false, true}) {
        auto cap = limits;
        cap.retained_bytes = changed.bytes.retained - (one_short ? 1u : 0u);
        DemoTilemapEditor editor;
        domain_view(editor);
        edit(editor, {key(Key::B)});
        editor.install_generated(recipe, Replace::DiscardCurrent, cap);
        const auto before = *editor.generated_state(0);
        const auto events =
            std::array{key(Key::Digit3), click(Mouse::Left, domain_cell(editor, 0, 0)), click(Mouse::Left, domain_cell(editor, 1, 0))};
        if (one_short) {
            EXPECT_THROW(editor.consume(events, editor_sizes), WorldProof::Error);
            expect_attachment(*editor.generated_state(0), before);
            expect_domain(editor, 0, initial.base, true);
        } else {
            editor.consume(events, editor_sizes);
            expect_domain(editor, 0, final, true);
            ASSERT_EQ(editor.generated_state(0)->overlay.edits.size(), 2u);
            const auto accepted = *editor.generated_state(0);
            const auto history = editor.history(0, Mode::Terrain);
            EXPECT_THROW(edit(editor, {click(Mouse::Left, domain_cell(editor, 2, 0))}), WorldProof::Error);
            expect_attachment(*editor.generated_state(0), accepted);
            EXPECT_EQ(editor.history(0, Mode::Terrain), history);
            edit(editor, {key(Key::F6)});
            EXPECT_TRUE(editor.generated_state(0)->overlay.edits.empty());
            edit(editor, {key(Key::F7)});
            expect_domain(editor, 0, final, true);
        }
    }
    // Same two-address command at the one-short EDIT cap, with a retained redo.
    {
        DemoTilemapEditor editor;
        domain_view(editor);
        edit(editor, {key(Key::B)});
        auto cap = limits;
        cap.edits = 1;
        editor.install_generated(recipe, Replace::DiscardCurrent, cap);
        const auto before = *editor.generated_state(0);
        EXPECT_THROW(
            edit(editor, {key(Key::Digit3), click(Mouse::Left, domain_cell(editor, 0, 0)), click(Mouse::Left, domain_cell(editor, 1, 0))}),
            WorldProof::Error
        );
        expect_attachment(*editor.generated_state(0), before);
        edit(editor, {click(Mouse::Left, domain_cell(editor, 0, 0)), key(Key::F6)});
        const auto redo = *editor.generated_state(0);
        EXPECT_THROW(
            edit(editor, {click(Mouse::Left, domain_cell(editor, 0, 0)), click(Mouse::Left, domain_cell(editor, 1, 0))}), WorldProof::Error
        );
        expect_attachment(*editor.generated_state(0), redo);
        edit(editor, {key(Key::F7)});
        auto one = initial.base;
        one.layers[0].cells[0] = "terrain:one";
        expect_domain(editor, 0, one, true);
    }
    // Even a fully prepared overlay cannot publish when its undo command cannot be retained.
    {
        DemoTilemapEditor editor(std::filesystem::path{}, {0, 1024 * 1024});
        domain_view(editor);
        edit(editor, {key(Key::B)});
        editor.install_generated(recipe, Replace::DiscardCurrent);
        const auto before = *editor.generated_state(0);
        EXPECT_THROW(edit(editor, {key(Key::Digit3), click(Mouse::Left, domain_cell(editor, 0, 0))}), WorldProof::Error);
        expect_attachment(*editor.generated_state(0), before);
        expect_domain(editor, 0, initial.base, true);
    }
    auto zero = limits;
    zero.edits = 0;
    auto empty = WorldProof::generate_document(recipe, zero);
    EXPECT_EQ(WorldProof::stage_generated_edits(empty, empty.base).overlay, empty.overlay);
    EXPECT_THROW(static_cast<void>(WorldProof::stage_generated_edits(empty, final)), WorldProof::Error);
    {
        DemoTilemapEditor editor;
        domain_view(editor);
        edit(editor, {key(Key::B)});
        editor.install_generated(recipe, Replace::DiscardCurrent, zero);
        const auto before = *editor.generated_state(0);
        edit(editor, {key(Key::Digit4), click(Mouse::Left, domain_cell(editor, 0, 0))}); // Existing label2, validated no-op.
        expect_attachment(*editor.generated_state(0), before);
    }
    for (auto invalid : {WorldProof::GeneratedLimits{1025}, WorldProof::GeneratedLimits{2, std::numeric_limits<std::size_t>::max()}})
        EXPECT_THROW(static_cast<void>(WorldProof::generate_document(recipe, invalid)), WorldProof::Error);
}

TEST(tilemap_consumer, generated_refusals) {
    const Fixture fixture;
    DemoTilemapEditor editor;
    domain_view(editor);
    edit(editor, {key(Key::B)});
    editor.install_generated(Generated::recipe(), Replace::DiscardCurrent);
    paint_generated(editor);
    const auto accepted = *editor.generated_state(0);
    const auto content = editor.terrain_document(0).snapshot();
    const auto* owner = &editor.terrain_document(0);
    for (int failure = 0; failure < 11; ++failure) {
        auto recipe = Generated::recipe();
        if (failure == 0)
            ++recipe.cave.random.seed;
        if (failure == 1)
            ++recipe.cave.random.stage;
        if (failure == 2)
            ++recipe.cave.requested.steps;
        if (failure == 3)
            recipe.cave.version = 2;
        if (failure == 4)
            recipe.cave.algorithm = static_cast<ProcGen::CaveAlgorithm>(99);
        if (failure == 5)
            recipe.adapter.version = 2;
        if (failure == 6)
            recipe.family = "dungeon";
        if (failure == 7)
            recipe.adapter.occupancy[0].key = "terrain:unknown";
        if (failure == 8)
            recipe.adapter.occupancy[0].label = {99};
        if (failure == 9)
            recipe.adapter.layers = {"other", "overlay"};
        if (failure == 10)
            recipe.adapter.spawns[0].x = 31;
        EXPECT_THROW(editor.install_generated(recipe, Replace::PreserveEdits), std::exception);
        EXPECT_EQ(&editor.terrain_document(0), owner);
        expect_domain(editor, 0, content, true);
        expect_attachment(*editor.generated_state(0), accepted);
    }
    editor.regenerate_selected();
    expect_attachment(*editor.generated_state(0), accepted);
    edit(editor, {key(Key::F6), key(Key::F7)});
    expect_domain(editor, 0, content, true);
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, generated_witness) {
    const auto accepted = WorldProof::generate_document(Generated::recipe(), {});
    const auto original = accepted;
    auto wrong = accepted;
    wrong.base.layers[0].cells[0] = "terrain:one"; // Same extent/metadata but a different canonical base.
    EXPECT_THROW(static_cast<void>(WorldProof::replay_generated(wrong, wrong.recipe)), WorldProof::Error);
    wrong.overlay = Patches::diff(wrong.base, wrong.base); // A real new witness STILL cannot match the original replay.
    EXPECT_THROW(static_cast<void>(WorldProof::replay_generated(wrong, wrong.recipe)), WorldProof::Error);
    auto target = accepted.base;
    target.id = "terrain:different";
    EXPECT_THROW(static_cast<void>(WorldProof::stage_generated_edits(accepted, target)), Patches::Error);
    target = accepted.base;
    std::swap(target.layers[0], target.layers[1]);
    EXPECT_THROW(static_cast<void>(WorldProof::stage_generated_edits(accepted, target)), Patches::Error);
    target = accepted.base;
    target.spawns.pop_back();
    EXPECT_THROW(static_cast<void>(WorldProof::stage_generated_edits(accepted, target)), Patches::Error);
    target = accepted.base;
    target.layers[1].cells.back() = "terrain:unknown";
    EXPECT_THROW(static_cast<void>(WorldProof::stage_generated_edits(accepted, target)), WorldProof::Error);
    auto changed = accepted;
    changed.limits.edits = 0;
    target = accepted.base;
    target.layers[1].cells.back() = "terrain:four";
    changed.overlay = Patches::diff(changed.base, target);
    EXPECT_THROW(static_cast<void>(WorldProof::replay_generated(changed, changed.recipe)), WorldProof::Error);
    expect_attachment(accepted, original);
    EXPECT_EQ(WorldProof::replay_generated(accepted, accepted.recipe), accepted.base);
}

TEST(tilemap_consumer, generated_late_adoption) {
    const Fixture fixture;
    auto palette = DemoTilemapEditor::diagnostic_terrain();
    palette.rule.cases.erase(2);
    DemoTilemapEditor editor(palette);
    domain_view(editor);
    edit(editor, {key(Key::B)});
    const auto recipe = small_recipe();
    editor.install_generated(recipe, Replace::DiscardCurrent);
    edit(editor, {key(Key::T), key(Key::Digit3), click(Mouse::Left, domain_cell(editor, 3, 2))});
    const auto held = *editor.generated_state(0);
    const auto before = editor.terrain_document(0).snapshot();
    Render::Frame frame;
    prepare_editor(frame, editor, fixture);
    const auto packets = frame.passes()[0].draws;
    auto proposal = recipe;
    proposal.adapter.initial_detail.resize(20);
    proposal.adapter.initial_detail[0] = proposal.adapter.initial_detail[1] = "terrain:one";
    const auto generated = WorldProof::generate_document(proposal, {});
    const auto first = WorldProof::TerrainOwner::from(
        generated.base, WorldProof::editor_meaning_ids(), ground, WorldProof::editor_semantics(), palette.rule, palette.target_to_id
    );
    ASSERT_EQ(first->terrain().tiles().extent(), (Maps::Extent{5, 4}));
    EXPECT_THROW(editor.install_generated(proposal, Replace::DiscardCurrent), bad_request);
    expect_attachment(*editor.generated_state(0), held);
    expect_domain(editor, 0, before, true);
    expect_packets(frame, packets);
    editor.regenerate_selected();
    edit(editor, {click(Mouse::Right, domain_cell(editor, 3, 2)), key(Key::F6)});
    expect_domain(editor, 0, before, true);
    fixture.calls->expect_cpu();
}

TEST(tilemap_consumer, generated_files) {
    const Fixture fixture;
    const DocumentFiles files;
    Render::Frame old;
    std::vector<Render::DrawPacket2D> packets;
    {
        DemoTilemapEditor editor(files.directory);
        domain_view(editor);
        edit(editor, {key(Key::B)});
        editor.install_generated(Generated::recipe(), Replace::DiscardCurrent);
        paint_generated(editor);
        auto held = *editor.generated_state(0);
        // Literal LOAD and another generated map must not detach this epoch.
        Documents::save(files.directory / "west.json", custom_document());
        edit(editor, {key(Key::V), key(Key::F9), key(Key::B), key(Key::Digit2)});
        expect_attachment(*editor.generated_state(0), held);
        editor.install_generated(Generated::recipe(1), Replace::DiscardCurrent);
        const auto east = *editor.generated_state(1);
        const auto east_content = editor.terrain_document(1).snapshot();
        edit(editor, {key(Key::Digit1)});
        expect_attachment(*editor.generated_state(0), held);

        prepare_editor(old, editor, fixture);
        packets = old.passes()[0].draws;
        edit(editor, {key(Key::F8)});
        EXPECT_EQ(Documents::load(files.directory / "west-terrain.json"), generated_final());
        expect_attachment(*editor.generated_state(0), held);
        // Owned recipe/base/witness/history counts vs FULL snapshot: no compactness claim.
        std::cout << "GENERATED RETAINED recipe=" << held.bytes.recipe << " base=" << held.bytes.base << " witness=" << held.bytes.witness
                  << " edits=" << held.bytes.edits << " history=" << held.bytes.history << " total=" << held.bytes.retained
                  << " full_snapshot=" << held.bytes.full_snapshot << " reserved_work=" << held.bytes.reserved_work
                  << " cave_storage=" << held.bytes.cave_storage << " cave_work=" << held.bytes.cave_work << '\n';
        raw_document(files.directory / "west-terrain.json", "{");
        EXPECT_THROW(edit(editor, {key(Key::F9)}), Documents::Error);
        expect_attachment(*editor.generated_state(0), held);
        Documents::save(files.directory / "predecessor.json", generated_final());
        std::filesystem::remove(files.directory / "west-terrain.json"); // Only this fixture's owned entry.
        std::filesystem::create_symlink(files.directory / "predecessor.json", files.directory / "west-terrain.json");
        const auto bytes = document_bytes(files.directory / "predecessor.json");
        EXPECT_THROW(edit(editor, {key(Key::F8)}), Documents::Error);
        EXPECT_EQ(document_bytes(files.directory / "predecessor.json"), bytes);
        expect_attachment(*editor.generated_state(0), held);
        std::filesystem::remove(files.directory / "west-terrain.json");
        Documents::save(files.directory / "west-terrain.json", generated_final());
        edit(editor, {key(Key::F9)});
        EXPECT_FALSE(editor.generated_state(0));
        expect_attachment(*editor.generated_state(1), east);
        EXPECT_EQ(editor.terrain_document(1).snapshot(), east_content);
        EXPECT_EQ(editor.document(0).snapshot(), custom_document());

        EXPECT_EQ(editor.history(0, Mode::Terrain), HistoryState{});
        expect_domain(editor, 0, generated_final(), true);
        EXPECT_THROW(editor.regenerate_selected(), WorldProof::Error);
    }
    expect_packets(old, packets);
    const DocumentFiles diagnostics;
    DemoTilemaps demo(diagnostics.directory);
    const std::array mode{key(Key::B)};
    demo.update(mode, editor_sizes);
    auto cap = WorldProof::GeneratedLimits{};
    cap.edits = 0;
    demo.install_generated(small_recipe(), Replace::DiscardCurrent, cap);
    const std::array tick{key(Key::Digit6), click(Mouse::Left, {40, 128}), key(Key::F8)};
    testing::internal::CaptureStderr();
    demo.update(tick, editor_sizes);
    const auto error = testing::internal::GetCapturedStderr();
    EXPECT_NE(error.find("generated document rejected"), std::string::npos);
    EXPECT_NE(error.find("changed-address cap"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(diagnostics.directory / "west-terrain.json"));
    const std::array save{key(Key::F8)};
    demo.update(save, editor_sizes);
    EXPECT_EQ(Documents::load(diagnostics.directory / "west-terrain.json"), WorldProof::generate_document(small_recipe(), cap).base);
    fixture.calls->expect_cpu();
}
