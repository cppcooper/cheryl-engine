#include <assets/selection/tile-selection.h>
#include <core/resources/asset-management/manifest-loader.h>
#include <gtest/gtest.h>
#include <internals/exceptions.h>

#include <array>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    using namespace CE::Assets;

    TerrainSampler uniform(const std::uint32_t terrain) {
        return [terrain](TerrainSite) { return TerrainSample{TerrainSampleState::Known, terrain}; };
    }

    WangAutotileDefinition wang_rule(const WangType type = WangType::Edge) {
        WangAutotileDefinition rule;
        rule.type = type;
        rule.slot_order = {Direction::North, Direction::NorthEast, Direction::East, Direction::SouthEast,
            Direction::South, Direction::SouthWest, Direction::West, Direction::NorthWest};
        TerrainDefinition first;
        first.id = 1;
        TerrainDefinition second;
        second.id = 2;
        rule.terrains = {first, second};
        return rule;
    }

    void add_variant(WangAutotileDefinition& rule, const CellIndex cell, const WangSignature& signature, const double weight = 1.0) {
        const auto index = rule.tiles.size();
        rule.tiles.push_back({cell, signature, weight});
        rule.variants[signature].push_back(index);
    }

    BitmaskAutotileDefinition bit_rule() {
        BitmaskAutotileDefinition rule;
        rule.type = BitmaskType::FourNeighbor;
        rule.bit_order = {Direction::West, Direction::North, Direction::East, Direction::South};
        rule.cases = {{0, 3}, {6, 13}, {15, 20}};
        return rule;
    }
}

TEST(tile_selection, wang_sites) {
    for (const auto type : {WangType::Edge, WangType::Corner}) {
        auto rule = wang_rule(type);
        const WangSignature signature = type == WangType::Edge ? WangSignature{1, 0, 2, 0, 1, 0, 2, 0} :
                                                                WangSignature{0, 1, 0, 2, 0, 1, 0, 2};
        add_variant(rule, 7, signature);
        std::vector<TerrainSite> calls;
        const TerrainSampler sampler = [&](const TerrainSite site) {
            calls.push_back(site);
            const bool vertical = site.direction == Direction::North || site.direction == Direction::NorthEast ||
                site.direction == Direction::South || site.direction == Direction::SouthWest;
            return TerrainSample{TerrainSampleState::Known, vertical ? 1u : 2u};
        };
        EXPECT_EQ(select_tile(rule, sampler, {}), (TileSelectionResult{CellIndex{7}}));
        const std::vector<TerrainSite> expected = type == WangType::Edge ?
            std::vector<TerrainSite>{{TerrainSiteKind::Edge, Direction::North}, {TerrainSiteKind::Edge, Direction::East},
                {TerrainSiteKind::Edge, Direction::South}, {TerrainSiteKind::Edge, Direction::West}} :
            std::vector<TerrainSite>{{TerrainSiteKind::Corner, Direction::NorthEast}, {TerrainSiteKind::Corner, Direction::SouthEast},
                {TerrainSiteKind::Corner, Direction::SouthWest}, {TerrainSiteKind::Corner, Direction::NorthWest}};
        EXPECT_EQ(calls, expected);
        const AutotileDefinition definition = rule;
        EXPECT_EQ(select_tile(definition, sampler, {}), (TileSelectionResult{CellIndex{7}}));
    }
}

TEST(tile_selection, bit_order) {
    const auto rule = bit_rule();
    std::vector<TerrainSite> calls;
    const TerrainSampler sampler = [&](const TerrainSite site) {
        calls.push_back(site);
        if (site.direction == Direction::North || site.direction == Direction::East)
            return TerrainSample{TerrainSampleState::Known, 1};
        return TerrainSample{TerrainSampleState::Known, site.direction == Direction::South ? 2u : 0u};
    };
    const TileSelectionOptions options{.terrain = 1};
    EXPECT_EQ(select_tile(rule, sampler, options), (TileSelectionResult{CellIndex{13}}));
    const std::vector<TerrainSite> expected{{TerrainSiteKind::Cell, Direction::North}, {TerrainSiteKind::Cell, Direction::East},
        {TerrainSiteKind::Cell, Direction::South}, {TerrainSiteKind::Cell, Direction::West}};
    EXPECT_EQ(calls, expected);
    const AutotileDefinition definition = rule;
    EXPECT_EQ(select_tile(definition, sampler, options), (TileSelectionResult{CellIndex{13}}));
    EXPECT_EQ(select_tile(rule, uniform(0), {}), (TileSelectionResult{CellIndex{3}})); // Empty never connects to empty.
}

TEST(tile_selection, diagonals) {
    BitmaskAutotileDefinition rule;
    rule.type = BitmaskType::EightNeighbor;
    rule.bit_order = {Direction::NorthEast};
    rule.cases = {{0, 3}, {1, 4}};
    std::vector<TerrainSite> calls;
    const TerrainSampler sampler = [&](const TerrainSite site) {
        calls.push_back(site);
        return TerrainSample{TerrainSampleState::Known, site.direction == Direction::East ? 2u : 1u};
    };
    TileSelectionOptions options{.terrain = 1};
    EXPECT_EQ(select_tile(rule, sampler, options), (TileSelectionResult{CellIndex{3}}));
    const std::vector<TerrainSite> gated{{TerrainSiteKind::Cell, Direction::North}, {TerrainSiteKind::Cell, Direction::NorthEast},
        {TerrainSiteKind::Cell, Direction::East}};
    EXPECT_EQ(calls, gated);
    EXPECT_EQ(select_tile(rule, uniform(1), options), (TileSelectionResult{CellIndex{4}}));
    options.diagonals = DiagonalConnectivity::Independent;
    calls.clear();
    EXPECT_EQ(select_tile(rule, sampler, options), (TileSelectionResult{CellIndex{4}}));
    EXPECT_EQ(calls, (std::vector<TerrainSite>{{TerrainSiteKind::Cell, Direction::NorthEast}}));

    rule.bit_order = {Direction::North, Direction::NorthEast, Direction::East, Direction::SouthEast,
        Direction::South, Direction::SouthWest, Direction::West, Direction::NorthWest};
    rule.cases.emplace(255, 8);
    options.diagonals = DiagonalConnectivity::RequireCardinals;
    EXPECT_EQ(select_tile(rule, uniform(1), options), (TileSelectionResult{CellIndex{8}}));
}

TEST(tile_selection, boundaries) {
    auto rule = bit_rule();
    rule.bit_order = {Direction::North};
    rule.cases = {{0, 3}, {1, 4}};
    TileSelectionOptions options{.terrain = 1};
    TerrainSampler sampler = [](TerrainSite) { return TerrainSample{TerrainSampleState::Outside, 99}; };
    EXPECT_EQ(select_tile(rule, sampler, options), (TileSelectionResult{CellIndex{3}}));
    options.outside = TerrainFallback::Center;
    EXPECT_EQ(select_tile(rule, sampler, options), (TileSelectionResult{CellIndex{4}}));
    options.outside = TerrainFallback::Unresolved;
    EXPECT_EQ(select_tile(rule, sampler, options), (TileSelectionResult{TileSelectionFailure::IncompleteNeighborhood}));

    sampler = [](TerrainSite) { return TerrainSample{}; };
    EXPECT_EQ(select_tile(rule, sampler, options), (TileSelectionResult{TileSelectionFailure::IncompleteNeighborhood}));
    options.unknown = TerrainFallback::Empty;
    EXPECT_EQ(select_tile(rule, sampler, options), (TileSelectionResult{CellIndex{3}}));
    options.unknown = TerrainFallback::Center;
    EXPECT_EQ(select_tile(rule, sampler, options), (TileSelectionResult{CellIndex{4}}));
}

TEST(tile_selection, unknown_wang) {
    auto rule = wang_rule();
    add_variant(rule, 3, {});
    add_variant(rule, 4, {1, 0, 1, 0, 1, 0, 1, 0});
    TileSelectionOptions options{.terrain = 1};
    EXPECT_EQ(select_tile(rule, uniform(0), options), (TileSelectionResult{CellIndex{3}}));
    EXPECT_EQ(select_tile(rule, uniform(99), options), (TileSelectionResult{TileSelectionFailure::IncompleteNeighborhood}));
    options.unknown = TerrainFallback::Empty;
    EXPECT_EQ(select_tile(rule, uniform(99), options), (TileSelectionResult{CellIndex{3}}));
    options.unknown = TerrainFallback::Center;
    EXPECT_EQ(select_tile(rule, uniform(99), options), (TileSelectionResult{CellIndex{4}}));
    options.terrain = 99;
    EXPECT_THROW(static_cast<void>(select_tile(rule, uniform(1), options)), CE::Exceptions::invalid_args);
}

TEST(tile_selection, missing) {
    auto bitmask = bit_rule();
    EXPECT_EQ(select_tile(bitmask, uniform(2), {.terrain = 2}), (TileSelectionResult{CellIndex{20}}));
    bitmask.cases.erase(15);
    EXPECT_EQ(select_tile(bitmask, uniform(2), {.terrain = 2}), (TileSelectionResult{TileSelectionFailure::MissingRule}));
    auto wang = wang_rule();
    EXPECT_EQ(select_tile(wang, uniform(1), {}), (TileSelectionResult{TileSelectionFailure::MissingRule}));
    wang.variants[{1, 0, 1, 0, 1, 0, 1, 0}] = {};
    EXPECT_EQ(select_tile(wang, uniform(1), {}), (TileSelectionResult{TileSelectionFailure::MissingRule}));

    std::size_t calls = 0;
    const TerrainSampler unavailable = [&](TerrainSite) {
        ++calls;
        return TerrainSample{};
    };
    EXPECT_EQ(select_tile(bitmask, unavailable, {}), (TileSelectionResult{TileSelectionFailure::IncompleteNeighborhood}));
    EXPECT_EQ(calls, 4u); // An unresolved sample does not skip later required sites.
    EXPECT_THROW(static_cast<void>(select_tile(bitmask, [](TerrainSite) -> TerrainSample { throw std::runtime_error("sample"); }, {})),
        std::runtime_error);
}

TEST(tile_selection, weights) {
    auto rule = wang_rule();
    const WangSignature signature{1, 0, 1, 0, 1, 0, 1, 0};
    add_variant(rule, 4, signature, 1.0);
    add_variant(rule, 9, signature, 3.0);
    std::array<CellIndex, 256> cells{};
    std::size_t heavy = 0;
    for (std::size_t seed = 0; seed < cells.size(); ++seed) {
        cells[seed] = std::get<CellIndex>(select_tile(rule, uniform(1), {.seed = seed}));
        EXPECT_TRUE(cells[seed] == 4 || cells[seed] == 9);
        if (cells[seed] == 9)
            ++heavy;
    }
    EXPECT_GT(heavy, 150u);
    EXPECT_LT(heavy, 230u);
    for (std::size_t index = cells.size(); index > 0; --index) {
        const auto seed = index - 1;
        EXPECT_EQ(select_tile(rule, uniform(1), {.seed = seed}), (TileSelectionResult{cells[seed]}));
    }
    rule.tiles[0].weight = 2.0;
    rule.tiles[1].weight = 6.0;
    for (std::size_t seed = 0; seed < cells.size(); ++seed)
        EXPECT_EQ(select_tile(rule, uniform(1), {.seed = seed}), (TileSelectionResult{cells[seed]}));

    rule.tiles[0].weight = std::numeric_limits<double>::max();
    rule.tiles[1].weight = std::numeric_limits<double>::max();
    const auto wide = rule;
    rule.tiles[0].weight = 1.0;
    rule.tiles[1].weight = 1.0;
    for (std::uint64_t seed = 0; seed < 32; ++seed)
        EXPECT_EQ(select_tile(wide, uniform(1), {.seed = seed}), select_tile(rule, uniform(1), {.seed = seed}));
}

TEST(tile_selection, invalid_bitmask) {
    auto rule = bit_rule();
    EXPECT_THROW(static_cast<void>(select_tile(rule, {}, {})), CE::Exceptions::invalid_args);
    EXPECT_THROW(static_cast<void>(select_tile(rule, uniform(1), {.unknown = static_cast<TerrainFallback>(99)})),
        CE::Exceptions::invalid_args);
    EXPECT_THROW(static_cast<void>(select_tile(rule, uniform(1), {.diagonals = static_cast<DiagonalConnectivity>(99)})),
        CE::Exceptions::invalid_args);
    const TerrainSampler invalid = [](TerrainSite) { return TerrainSample{static_cast<TerrainSampleState>(99), 0}; };
    EXPECT_THROW(static_cast<void>(select_tile(rule, invalid, {})), CE::Exceptions::invalid_args);
    rule.bit_order = {Direction::North, Direction::North};
    EXPECT_THROW(static_cast<void>(select_tile(rule, uniform(1), {})), CE::Exceptions::invalid_args);
    rule.bit_order = {Direction::NorthEast};
    EXPECT_THROW(static_cast<void>(select_tile(rule, uniform(1), {})), CE::Exceptions::invalid_args);
    rule.bit_order = {static_cast<Direction>(99)};
    EXPECT_THROW(static_cast<void>(select_tile(rule, uniform(1), {})), CE::Exceptions::invalid_args);
    rule.bit_order.clear();
    EXPECT_THROW(static_cast<void>(select_tile(rule, uniform(1), {})), CE::Exceptions::invalid_args);
}

TEST(tile_selection, invalid_wang) {
    auto rule = wang_rule();
    const WangSignature signature{1, 0, 1, 0, 1, 0, 1, 0};
    add_variant(rule, 4, signature);
    add_variant(rule, 9, signature);
    for (const auto weight : {0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        rule.tiles[1].weight = weight;
        EXPECT_THROW(static_cast<void>(select_tile(rule, uniform(1), {})), CE::Exceptions::invalid_args);
    }
    rule.tiles[1].weight = 1.0;
    rule.tiles[0].wang[0] = 2;
    EXPECT_THROW(static_cast<void>(select_tile(rule, uniform(1), {})), CE::Exceptions::invalid_args);
    rule.tiles[0].wang = signature;
    rule.variants[signature] = {2};
    EXPECT_THROW(static_cast<void>(select_tile(rule, uniform(1), {})), CE::Exceptions::invalid_args);
    rule.variants[signature] = {0, 0};
    EXPECT_THROW(static_cast<void>(select_tile(rule, uniform(1), {})), CE::Exceptions::invalid_args);
    rule.variants[signature] = {1, 0};
    EXPECT_THROW(static_cast<void>(select_tile(rule, uniform(1), {})), CE::Exceptions::invalid_args);
    rule.variants[signature] = {0};
    rule.slot_order[0] = Direction::South;
    EXPECT_THROW(static_cast<void>(select_tile(rule, uniform(1), {})), CE::Exceptions::invalid_args);
}

TEST(tile_selection, manifest_order) {
    const auto parse = [](const std::string& type, const std::string& order) {
        std::istringstream input(R"json({"$schema":"./schemas/asset-manifest-1.0.schema.json",
          "version":"1.0","namespace":"test","texture":"sheet.png",
          "defaults":{"sprite":{"pivot":{"x":0.5,"y":0.5}},"tileset":{"pivot":{"x":0.5,"y":0.5}}},
          "tilesets":{"terrain":{"grid":{"origin":{"x":0,"y":0},"frame":{"width":16,"height":16},
          "spacing":{"x":0,"y":0},"rows":1,"columns":1,"cell_order":"row-major"},
          "autotiles":{"rule":{"type":")json" + type + R"json(","bit_order":)json" + order +
            R"json(,"cases":{"0":{"index":0}}}}}}})json");
        return ManifestLoader::parse(input, "bitmask.json");
    };
    EXPECT_THROW(static_cast<void>(parse("four-neighbor", R"(["north_east"])")), CE::Exceptions::runtime_exception);
    EXPECT_THROW(static_cast<void>(parse("four-neighbor", R"(["north","east","south","west","north_east"])")),
        CE::Exceptions::runtime_exception);
    const auto manifest = parse("eight-neighbor", R"(["north_east"])");
    const auto& rule = std::get<BitmaskAutotileDefinition>(manifest.tilesets.front().autotiles.at("rule"));
    EXPECT_EQ(rule.bit_order, (std::vector{Direction::NorthEast}));
}
