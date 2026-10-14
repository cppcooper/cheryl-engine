#include <assets/types/2d/tileset.h>
#include <gtest/gtest.h>
#include <internals/exceptions.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
    using namespace std::chrono_literals;

    CE::Assets::Tilesets::Asset make_tileset() {
        CE::Assets::Tilesets::Definition definition;
        definition.grid.rows = 1;
        definition.grid.columns = 16;
        CE::Assets::Tilesets::Clip short_clip;
        short_clip.target = 4;
        short_clip.frames = {{1, 30ms}, {2, 50ms}};
        definition.animations.emplace("short", short_clip);
        short_clip.target = 2;
        short_clip.frames = {{9, 20ms}};
        definition.animations.emplace("frame_target", short_clip);
        short_clip.target = 12;
        short_clip.frames = {{8, 30ms}, {9, 50ms}};
        short_clip.loop = true;
        definition.animations.emplace("loop", short_clip);

        CE::Assets::Tilesets::BitmaskRule bitmask;
        bitmask.type = CE::Assets::Tilesets::BitmaskType::FourNeighbor;
        bitmask.bit_order = {CE::Assets::Tilesets::Direction::North};
        bitmask.cases = {{0, 4}, {1, 12}};
        definition.autotiles.emplace("border", bitmask);

        CE::Assets::Tilesets::WangRule wang;
        wang.type = CE::Assets::Tilesets::WangType::Edge;
        wang.slot_order = {CE::Assets::Tilesets::Direction::North, CE::Assets::Tilesets::Direction::NorthEast,
                           CE::Assets::Tilesets::Direction::East,  CE::Assets::Tilesets::Direction::SouthEast,
                           CE::Assets::Tilesets::Direction::South, CE::Assets::Tilesets::Direction::SouthWest,
                           CE::Assets::Tilesets::Direction::West,  CE::Assets::Tilesets::Direction::NorthWest};
        CE::Assets::Tilesets::Terrain terrain;
        terrain.id = 1;
        wang.terrains.push_back(terrain);
        const CE::Assets::Tilesets::WangSignature signature{1, 0, 1, 0, 1, 0, 1, 0};
        wang.tiles = {{4, signature, 1.0}, {12, signature, 1.0}};
        wang.variants.emplace(signature, std::vector<std::size_t>{0, 1});
        definition.autotiles.emplace("terrain", wang);
        return CE::Assets::Tilesets::Asset(CE::Assets::Tilesets::Data{nullptr, nullptr, std::move(definition)});
    }

    CE::Assets::Tilesets::Selection::Sampler uniform() {
        return [](CE::Assets::Tilesets::Selection::Site) {
            return CE::Assets::Tilesets::Selection::Sample{CE::Assets::Tilesets::Selection::SampleState::Known, 1};
        };
    }
}

TEST(tileset_selection, timing) {
    const auto tileset = make_tileset();
    EXPECT_EQ(tileset.cell_at(4, 0ms), 1u);
    EXPECT_EQ(tileset.cell_at(4, 29ms), 1u);
    EXPECT_EQ(tileset.cell_at(4, 30ms), 2u);
    EXPECT_EQ(tileset.cell_at(4, 80ms), 2u);
    EXPECT_EQ(tileset.cell_at(2, 0ms), 9u); // CE::Assets::Animation::Cursor 2 is a target, but selecting target 4 never recurses into it.
    EXPECT_EQ(tileset.cell_at(12, 80ms), 8u);
    EXPECT_EQ(tileset.cell_at(12, 110ms), 9u);
    EXPECT_EQ(tileset.cell_at(15, std::chrono::milliseconds::max()), 15u);
}

TEST(tileset_selection, phase) {
    const auto tileset = make_tileset();
    const auto sampler = uniform();
    bool saw_short = false;
    bool saw_loop = false;
    for (std::uint64_t seed = 0; seed < 64; ++seed) {
        const CE::Assets::Tilesets::Selection::Options options{.terrain = 1, .seed = seed};
        const auto first =
            std::get<CE::Assets::Grids::CellIndex>(tileset.select_tile("terrain", sampler, options, 0ms));
        EXPECT_TRUE(first == 1 || first == 8);
        saw_short = saw_short || first == 1;
        saw_loop = saw_loop || first == 8;
        EXPECT_EQ(
            tileset.select_tile("terrain", sampler, options, 30ms),
            (CE::Assets::Tilesets::Selection::Result{CE::Assets::Grids::CellIndex{first == 1 ? 2u : 9u}})
        );
        EXPECT_EQ(
            tileset.select_tile("terrain", sampler, options, 80ms),
            (CE::Assets::Tilesets::Selection::Result{CE::Assets::Grids::CellIndex{first == 1 ? 2u : 8u}})
        );
        EXPECT_EQ(
            tileset.select_tile("terrain", sampler, options, 0ms),
            (CE::Assets::Tilesets::Selection::Result{first})
        );
    }
    EXPECT_TRUE(saw_short);
    EXPECT_TRUE(saw_loop);
}

TEST(tileset_selection, failures) {
    const auto original = make_tileset();
    const CE::Assets::Tilesets::Selection::Sampler unavailable = [](CE::Assets::Tilesets::Selection::Site) {
        return CE::Assets::Tilesets::Selection::Sample{};
    };
    EXPECT_EQ(
        original.select_tile("border", unavailable, {}, 0ms),
        (CE::Assets::Tilesets::Selection::Result{CE::Assets::Tilesets::Selection::Failure::IncompleteNeighborhood})
    );
    EXPECT_THROW(static_cast<void>(original.select_tile("absent", uniform(), {}, 0ms)), std::out_of_range);
    auto definition = original.definition();
    std::get<CE::Assets::Tilesets::BitmaskRule>(definition.autotiles.at("border")).cases.erase(1);
    definition.animations.at("short").frames.clear();
    const CE::Assets::Tilesets::Asset missing(CE::Assets::Tilesets::Data{nullptr, nullptr, std::move(definition)});
    EXPECT_EQ(
        missing.select_tile("border", uniform(), {.terrain = 1}, 0ms),
        (CE::Assets::Tilesets::Selection::Result{CE::Assets::Tilesets::Selection::Failure::MissingRule})
    );
}

TEST(tileset_selection, bounds) {
    const auto original = make_tileset();
    EXPECT_THROW(static_cast<void>(original.cell_at(16, 0ms)), CE::Exceptions::bad_request);
    auto definition = original.definition();
    definition.animations.at("short").frames[1].cell = 16;
    const CE::Assets::Tilesets::Asset frame(CE::Assets::Tilesets::Data{nullptr, nullptr, definition});
    EXPECT_EQ(frame.cell_at(4, 0ms), 1u);
    EXPECT_THROW(static_cast<void>(frame.cell_at(4, 30ms)), CE::Exceptions::bad_request);
    EXPECT_THROW(
        static_cast<void>(frame.select_tile(
            "border",
            [](CE::Assets::Tilesets::Selection::Site) {
                return CE::Assets::Tilesets::Selection::Sample{CE::Assets::Tilesets::Selection::SampleState::Outside};
            },
            {}, 30ms
        )),
        CE::Exceptions::bad_request
    );

    std::get<CE::Assets::Tilesets::BitmaskRule>(definition.autotiles.at("border")).cases[1] = 16;
    auto outside_clip = definition.animations.at("loop");
    outside_clip.target = 16;
    definition.animations.emplace("outside", outside_clip);
    const CE::Assets::Tilesets::Asset target(CE::Assets::Tilesets::Data{nullptr, nullptr, std::move(definition)});
    EXPECT_THROW(
        static_cast<void>(target.select_tile("border", uniform(), {.terrain = 1}, 0ms)),
        CE::Exceptions::bad_request
    );
}

TEST(tileset_selection, invalid_time) {
    const auto original = make_tileset();
    EXPECT_THROW(static_cast<void>(original.cell_at(15, -1ms)), CE::Exceptions::invalid_args);
    EXPECT_THROW(static_cast<void>(original.cell_at(4, -1ms)), CE::Exceptions::invalid_args);
    std::size_t calls = 0;
    const CE::Assets::Tilesets::Selection::Sampler sampler = [&](CE::Assets::Tilesets::Selection::Site) {
        ++calls;
        return CE::Assets::Tilesets::Selection::Sample{};
    };
    EXPECT_THROW(
        static_cast<void>(original.select_tile("border", sampler, {}, -1ms)), CE::Exceptions::invalid_args
    );
    EXPECT_EQ(calls, 0u);

    auto definition = original.definition();
    definition.animations.at("short").frames[1].duration = 0ms;
    const CE::Assets::Tilesets::Asset timeline(CE::Assets::Tilesets::Data{nullptr, nullptr, std::move(definition)});
    EXPECT_THROW(static_cast<void>(timeline.cell_at(4, 0ms)), CE::Exceptions::invalid_args);
}
