#include <cheryl/maps/terrain-map.h>

#include <gtest/gtest.h>
#include <internals/exceptions.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <limits>
#include <string>
#include <utility>

namespace {
    namespace Maps = CE::Tilemaps;
    namespace Tiles = CE::Assets::Tilesets;
    using CE::Exceptions::bad_request;
    using CE::Exceptions::invalid_args;
    using CE::Tilemaps::TerrainLabel;
    using CE::Tilemaps::TerrainMap;
    constexpr Maps::LayerIndex ground{0}, detail{1};

    Tiles::BitmaskRule cardinal() {
        Tiles::BitmaskRule rule;
        rule.type = Tiles::BitmaskType::FourNeighbor;
        rule.bit_order = {Tiles::Direction::North, Tiles::Direction::East, Tiles::Direction::South, Tiles::Direction::West};
        for (std::uint32_t mask = 0; mask < 16; ++mask)
            rule.cases.emplace(mask, mask);
        return rule;
    }

    std::vector<Maps::TileId> ids() {
        std::vector<Maps::TileId> result;
        for (std::uint32_t mask = 0; mask < 16; ++mask)
            result.push_back({100 + mask});
        return result;
    }

    std::vector<TerrainLabel> labels(const TerrainMap& map) {
        std::vector<TerrainLabel> result;
        for (std::int64_t row = 0; row < map.tiles().extent().rows; ++row)
            for (std::int64_t column = 0; column < map.tiles().extent().columns; ++column)
                result.push_back(map.label({column, row}));
        return result;
    }

    std::vector<Maps::TileId> cells(const Maps::Map& map, const Maps::LayerIndex layer = ground) {
        std::vector<Maps::TileId> result;
        for (std::int64_t row = 0; row < map.extent().rows; ++row)
            for (std::int64_t column = 0; column < map.extent().columns; ++column)
                result.push_back(map.get(layer, {column, row}));
        return result;
    }

    // Full-grid oracle: enumerate each rectangle/cell and test semantic neighbors
    // explicitly. No incremental candidate list, helper resolve or select() call.
    std::vector<Maps::TileId> reference(
        const Maps::Extent extent,
        const std::vector<TerrainLabel>& semantic,
        const Tiles::BitmaskRule& rule,
        const std::vector<Maps::TileId>& mapping
    ) {
        std::vector<Maps::TileId> output;
        for (std::uint32_t row = 0; row < extent.rows; ++row) {
            for (std::uint32_t column = 0; column < extent.columns; ++column) {
                const auto index = static_cast<std::size_t>(row) * extent.columns + column;
                const auto center = semantic.at(index);
                if (center.value == 0) {
                    output.push_back({0});
                    continue;
                }
                const bool north = row > 0 && semantic.at(index - extent.columns) == center;
                const bool east = column + 1 < extent.columns && semantic.at(index + 1) == center;
                const bool south = row + 1 < extent.rows && semantic.at(index + extent.columns) == center;
                const bool west = column > 0 && semantic.at(index - 1) == center;
                std::uint32_t mask = 0;
                std::uint32_t weight = 1;
                for (const auto direction : rule.bit_order) {
                    const bool joined = direction == Tiles::Direction::North   ? north
                                        : direction == Tiles::Direction::East  ? east
                                        : direction == Tiles::Direction::South ? south
                                                                               : west;
                    if (joined)
                        mask += weight;
                    weight *= 2;
                }
                output.push_back(mapping.at(rule.cases.at(mask)));
            }
        }
        return output;
    }

    std::vector<Maps::Point> sorted(std::vector<Maps::Point> points) {
        std::ranges::sort(points, [](const auto a, const auto b) { return a.row == b.row ? a.column < b.column : a.row < b.row; });
        return points;
    }

    std::string refusal(TerrainMap& map, const Maps::Point point, const TerrainLabel value) {
        try {
            static_cast<void>(map.edit(point, value));
            ADD_FAILURE() << "Expected a refused terrain edit";
        } catch (const bad_request& error) {
            return error.what();
        }
        return {};
    }
}

TEST(tilemap_terrain, masks) {
    const Maps::Map base({3, 3}, 2);
    const std::vector<TerrainLabel> full(9, {1});
    const TerrainMap map(base, ground, full, cardinal(), ids());
    // Literal N/E/S/W masks (bits 1/2/4/8), including corners, edges, interior.
    const std::vector<Maps::TileId> expected{{106}, {114}, {112}, {107}, {115}, {113}, {103}, {111}, {109}};
    EXPECT_EQ(cells(map.tiles()), expected);
    const Maps::Map asymmetric({3, 2}, 2);
    const TerrainMap rectangle(asymmetric, ground, std::vector<TerrainLabel>(6, {1}), cardinal(), ids());
    EXPECT_EQ(cells(rectangle.tiles()), (std::vector<Maps::TileId>{{106}, {114}, {112}, {103}, {111}, {109}}));
    const std::vector<TerrainLabel> isolated{{0}, {0}, {0}, {0}, {42}, {0}, {0}, {0}, {0}};
    const TerrainMap one(base, ground, isolated, cardinal(), ids());
    EXPECT_EQ(cells(one.tiles()), (std::vector<Maps::TileId>{{0}, {0}, {0}, {0}, {100}, {0}, {0}, {0}, {0}}));
    EXPECT_EQ(cells(base), std::vector<Maps::TileId>(9));
}

TEST(tilemap_terrain, bit_order) {
    auto rule = cardinal();
    rule.bit_order = {Tiles::Direction::West, Tiles::Direction::South, Tiles::Direction::North, Tiles::Direction::East};
    const Maps::Map base({3, 3}, 1);
    const TerrainMap map(base, ground, std::vector<TerrainLabel>(9, {1}), rule, ids());
    // W/S/N/E positions: W=1, S=2, N=4, E=8; these are literal expectations.
    EXPECT_EQ(cells(map.tiles()), (std::vector<Maps::TileId>{{110}, {111}, {103}, {114}, {115}, {107}, {112}, {113}, {105}}));
    rule.bit_order = {Tiles::Direction::East, Tiles::Direction::North};
    rule.cases = {{0, 0}, {1, 1}, {2, 2}, {3, 3}};
    const TerrainMap subset(Maps::Map({2, 2}, 1), ground, std::vector<TerrainLabel>(4, {1}), rule, ids());
    EXPECT_EQ(cells(subset.tiles()), (std::vector<Maps::TileId>{{101}, {100}, {103}, {102}}));
}

TEST(tilemap_terrain, semantics) {
    const Maps::Map pair({2, 1}, 2);
    const std::vector<TerrainLabel> different{{1}, {2}};
    auto mapping = ids();
    mapping[0] = {7};
    mapping[2] = mapping[8] = {8};
    TerrainMap map(pair, ground, different, cardinal(), mapping);
    EXPECT_EQ(cells(map.tiles()), (std::vector<Maps::TileId>{{7}, {7}})); // Different labels, same sprite.
    static_cast<void>(map.edit({1, 0}, {std::numeric_limits<std::uint32_t>::max()}));
    EXPECT_EQ(cells(map.tiles()), (std::vector<Maps::TileId>{{7}, {7}})); // Visual equality must not connect.
    static_cast<void>(map.edit({1, 0}, {1}));
    EXPECT_EQ(cells(map.tiles()), (std::vector<Maps::TileId>{{8}, {8}}));
    const Maps::Map line({3, 1}, 1);
    TerrainMap connected(line, ground, std::vector<TerrainLabel>(3, {1}), cardinal(), ids());
    EXPECT_EQ(cells(connected.tiles()), (std::vector<Maps::TileId>{{102}, {110}, {108}})); // One label, three distinct IDs.
    static_cast<void>(connected.edit({1, 0}, {2}));
    EXPECT_EQ(cells(connected.tiles()), (std::vector<Maps::TileId>{{100}, {100}, {100}}));
}

TEST(tilemap_terrain, invalidation) {
    TerrainMap map(Maps::Map({3, 3}, 2), ground, std::vector<TerrainLabel>(9, {1}), cardinal(), ids());
    EXPECT_EQ(map.edit({1, 1}, {2}), (std::vector<Maps::Point>{{1, 1}, {1, 0}, {2, 1}, {1, 2}, {0, 1}}));
    EXPECT_EQ(cells(map.tiles()), (std::vector<Maps::TileId>{{106}, {110}, {112}, {105}, {100}, {105}, {103}, {110}, {109}}));
    EXPECT_TRUE(map.edit({1, 1}, {2}).empty()); // No invalidation or work for the same label.
    EXPECT_EQ(map.edit({0, 0}, {0}), (std::vector<Maps::Point>{{0, 0}, {1, 0}, {0, 1}}));
    EXPECT_EQ(map.edit({0, 1}, {0}), (std::vector<Maps::Point>{{0, 1}, {0, 0}, {1, 1}, {0, 2}}));
    EXPECT_EQ(cells(map.tiles(), detail), std::vector<Maps::TileId>(9));
}

TEST(tilemap_terrain, sequences) {
    const std::array extents{Maps::Extent{5, 3}, Maps::Extent{1, 4}, Maps::Extent{4, 1}, Maps::Extent{1, 1}};
    for (const auto extent : extents) {
        for (const auto noncanonical : {false, true}) {
            auto rule = cardinal();
            if (noncanonical)
                rule.bit_order = {Tiles::Direction::South, Tiles::Direction::West, Tiles::Direction::East, Tiles::Direction::North};
            const auto mapping = ids();
            const auto count = static_cast<std::size_t>(extent.columns) * extent.rows;
            std::vector<TerrainLabel> semantic(count);
            TerrainMap map(Maps::Map(extent, 2), ground, semantic, rule, mapping);
            for (std::size_t step = 0; step < 48; ++step) {
                const auto index = (step * 7 + step / 3) % count;
                const Maps::Point point{
                    static_cast<std::int64_t>(index % extent.columns), static_cast<std::int64_t>(index / extent.columns)
                };
                const TerrainLabel value{static_cast<std::uint32_t>((step / 2) % 3)};
                const auto unchanged = semantic[index] == value;
                const auto affected = map.edit(point, value);
                semantic[index] = value;
                // Independently enumerate the entire grid and select Manhattan
                // distance <= 1; do not mirror the helper's offset candidate list.
                std::vector<Maps::Point> expected;
                for (std::int64_t row = 0; row < extent.rows; ++row)
                    for (std::int64_t column = 0; column < extent.columns; ++column)
                        if (!unchanged && std::abs(column - point.column) + std::abs(row - point.row) <= 1)
                            expected.push_back({column, row});
                EXPECT_EQ(sorted(affected), expected);
                EXPECT_EQ(std::unique(expected.begin(), expected.end()), expected.end());
                EXPECT_EQ(labels(map), semantic);
                EXPECT_EQ(cells(map.tiles()), reference(extent, semantic, rule, mapping));
                EXPECT_EQ(cells(map.tiles(), detail), std::vector<Maps::TileId>(count));
            }
        }
    }
}

TEST(tilemap_terrain, failures) {
    const Maps::Map base({3, 1}, 2);
    const std::vector<TerrainLabel> initial(3, {1});
    auto rule = cardinal();
    rule.cases = {{2, 0}, {10, 1}, {8, 2}};
    const std::vector<Maps::TileId> mapping{{7}, {8}, {9}};
    TerrainMap missing(base, ground, initial, rule, mapping); // Initial masks all resolve.
    const auto before = cells(missing.tiles());
    const auto error = refusal(missing, {1, 0}, {0}); // Empty center is valid; later east neighbor needs absent mask0.
    EXPECT_NE(error.find("missing rule at (2,0)"), std::string::npos);
    EXPECT_EQ(labels(missing), initial);
    EXPECT_EQ(cells(missing.tiles()), before);
    rule.cases[0] = 3;
    TerrainMap invalid_target(base, ground, initial, rule, mapping);
    EXPECT_NE(refusal(invalid_target, {1, 0}, {0}).find("target 3"), std::string::npos);
    EXPECT_EQ(labels(invalid_target), initial);
    EXPECT_EQ(cells(invalid_target.tiles()), before);
    auto hole = mapping;
    hole.push_back({0});
    TerrainMap unmapped(base, ground, initial, rule, hole);
    EXPECT_NE(refusal(unmapped, {1, 0}, {0}).find("unmapped at (2,0)"), std::string::npos);
    EXPECT_EQ(labels(unmapped), initial);
    EXPECT_EQ(cells(unmapped.tiles()), before);
    hole[3] = {10};
    TerrainMap valid(base, ground, initial, rule, hole); // Prove the same complete edit succeeds with the missing mapping supplied.
    EXPECT_EQ(valid.edit({1, 0}, {0}), (std::vector<Maps::Point>{{1, 0}, {2, 0}, {0, 0}}));
    EXPECT_EQ(cells(valid.tiles()), (std::vector<Maps::TileId>{{10}, {0}, {10}}));
    for (const auto point :
         {Maps::Point{-1, 0}, Maps::Point{0, -1}, Maps::Point{3, 0}, Maps::Point{0, 1},
          Maps::Point{std::numeric_limits<std::int64_t>::min(), 0}, Maps::Point{0, std::numeric_limits<std::int64_t>::max()}}) {
        EXPECT_THROW(static_cast<void>(missing.edit(point, {0})), bad_request);
        EXPECT_THROW(static_cast<void>(missing.label(point)), bad_request);
    }
    EXPECT_EQ(labels(missing), initial);
    EXPECT_EQ(cells(missing.tiles()), before);
    EXPECT_EQ(cells(base), std::vector<Maps::TileId>(3));
}

TEST(tilemap_terrain, metadata) {
    Maps::Map base({2, 1}, 2);
    base.set(detail, {0, 0}, {42});
    const auto base_ground = cells(base);
    const auto base_detail = cells(base, detail);
    std::vector<TerrainLabel> semantic{{1}, {2}};
    auto rule = cardinal();
    auto mapping = ids();
    TerrainMap owned(base, ground, semantic, rule, mapping);
    semantic.assign(2, {0});
    rule.cases.clear();
    mapping.clear();
    base.set(detail, {0, 0}, {99});
    EXPECT_EQ(owned.edit({1, 0}, {1}), (std::vector<Maps::Point>{{1, 0}, {0, 0}}));
    EXPECT_EQ(cells(owned.tiles()), (std::vector<Maps::TileId>{{102}, {108}}));
    EXPECT_EQ(cells(owned.tiles(), detail), base_detail);
    EXPECT_THROW(static_cast<void>(TerrainMap(base, ground, std::vector<TerrainLabel>(1), cardinal(), ids())), invalid_args);
    EXPECT_THROW(static_cast<void>(TerrainMap(base, {-1}, semantic, cardinal(), ids())), bad_request);
    EXPECT_THROW(static_cast<void>(TerrainMap(base, {2}, semantic, cardinal(), ids())), bad_request);
    auto eight = cardinal();
    eight.type = Tiles::BitmaskType::EightNeighbor;
    EXPECT_THROW(static_cast<void>(TerrainMap(base, ground, semantic, eight, ids())), invalid_args);
    EXPECT_THROW(static_cast<void>(TerrainMap(base, ground, semantic, Tiles::WangRule{}, ids())), invalid_args);
    for (const auto& order :
         {std::vector<Tiles::Direction>{}, std::vector<Tiles::Direction>{Tiles::Direction::North, Tiles::Direction::North},
          std::vector<Tiles::Direction>{Tiles::Direction::NorthEast}, std::vector<Tiles::Direction>{static_cast<Tiles::Direction>(-1)}}) {
        auto invalid = cardinal();
        invalid.bit_order = order;
        EXPECT_THROW(static_cast<void>(TerrainMap(base, ground, semantic, invalid, ids())), invalid_args);
    }
    auto invalid = cardinal();
    invalid.cases[16] = 0;
    EXPECT_THROW(static_cast<void>(TerrainMap(base, ground, semantic, invalid, ids())), invalid_args);
    auto target = cardinal();
    target.cases[0] = std::numeric_limits<CE::Assets::Grids::CellIndex>::max();
    semantic = {{1}, {2}};
    EXPECT_THROW(static_cast<void>(TerrainMap(base, ground, semantic, target, ids())), bad_request);
    Maps::Map moved(std::move(base));
    EXPECT_THROW(static_cast<void>(TerrainMap(base, ground, semantic, cardinal(), ids())), invalid_args);
    EXPECT_EQ(cells(moved), base_ground);
    EXPECT_EQ(cells(owned.tiles(), detail), base_detail);
}

TEST(tilemap_terrain, empty) {
    const auto rule = cardinal();
    const auto mapping = ids();
    TerrainMap single(Maps::Map({1, 1}, 2), ground, std::array<TerrainLabel, 1>{{{9}}}, rule, mapping);
    EXPECT_EQ(single.tiles().get(ground, {0, 0}), (Maps::TileId{100})); // Four outside samples substitute empty, never wrap/center.
    EXPECT_EQ(single.edit({0, 0}, {0}), (std::vector<Maps::Point>{{0, 0}}));
    EXPECT_EQ(single.tiles().get(ground, {0, 0}), (Maps::TileId{0}));
    EXPECT_TRUE(single.edit({0, 0}, {0}).empty());
    const TerrainMap vertical(Maps::Map({1, 2}, 1), ground, std::vector<TerrainLabel>(2, {9}), rule, mapping);
    EXPECT_EQ(cells(vertical.tiles()), (std::vector<Maps::TileId>{{104}, {101}}));
    auto sparse = rule;
    sparse.cases.clear();
    TerrainMap empty(Maps::Map({1, 1}, 1), ground, std::array<TerrainLabel, 1>{}, sparse, {});
    EXPECT_TRUE(empty.edit({0, 0}, {0}).empty());
    EXPECT_THROW(static_cast<void>(empty.edit({0, 0}, {1})), bad_request);
    EXPECT_EQ(empty.label({0, 0}), (TerrainLabel{0}));
    EXPECT_EQ(empty.tiles().get(ground, {0, 0}), (Maps::TileId{0}));
    const std::vector<Maps::TileId> zero{{0}};
    TerrainMap hole(Maps::Map({1, 1}, 1), ground, std::array<TerrainLabel, 1>{}, rule, zero);
    EXPECT_THROW(static_cast<void>(hole.edit({0, 0}, {1})), bad_request);
    const std::vector<Maps::TileId> arbitrary{{std::numeric_limits<std::uint32_t>::max()}};
    TerrainMap logical(Maps::Map({1, 1}, 1), ground, std::array<TerrainLabel, 1>{{{9}}}, rule, arbitrary);
    EXPECT_EQ(logical.tiles().get(ground, {0, 0}), arbitrary[0]); // Every nonzero logical ID is valid here; retained binding is separate.
}
