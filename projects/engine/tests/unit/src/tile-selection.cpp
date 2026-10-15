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

    CE::Assets::Tilesets::Selection::Sampler uniform(const std::uint32_t terrain) {
        return [terrain](CE::Assets::Tilesets::Selection::Site) {
            return CE::Assets::Tilesets::Selection::Sample{CE::Assets::Tilesets::Selection::SampleState::Known, terrain};
        };
    }

    CE::Assets::Tilesets::WangRule wang_rule(const CE::Assets::Tilesets::WangType type = CE::Assets::Tilesets::WangType::Edge) {
        CE::Assets::Tilesets::WangRule rule;
        rule.type = type;
        rule.slot_order = {CE::Assets::Tilesets::Direction::North, CE::Assets::Tilesets::Direction::NorthEast,
                           CE::Assets::Tilesets::Direction::East,  CE::Assets::Tilesets::Direction::SouthEast,
                           CE::Assets::Tilesets::Direction::South, CE::Assets::Tilesets::Direction::SouthWest,
                           CE::Assets::Tilesets::Direction::West,  CE::Assets::Tilesets::Direction::NorthWest};
        CE::Assets::Tilesets::Terrain first;
        first.id = 1;
        CE::Assets::Tilesets::Terrain second;
        second.id = 2;
        rule.terrains = {first, second};
        return rule;
    }

    void add_variant(
        CE::Assets::Tilesets::WangRule& rule,
        const CE::Assets::Grids::CellIndex cell,
        const CE::Assets::Tilesets::WangSignature& signature,
        const double weight = 1.0
    ) {
        const auto index = rule.tiles.size();
        rule.tiles.push_back({cell, signature, weight});
        rule.variants[signature].push_back(index);
    }

    CE::Assets::Tilesets::BitmaskRule bit_rule() {
        CE::Assets::Tilesets::BitmaskRule rule;
        rule.type = CE::Assets::Tilesets::BitmaskType::FourNeighbor;
        rule.bit_order = {
            CE::Assets::Tilesets::Direction::West, CE::Assets::Tilesets::Direction::North, CE::Assets::Tilesets::Direction::East,
            CE::Assets::Tilesets::Direction::South
        };
        rule.cases = {{0, 3}, {6, 13}, {15, 20}};
        return rule;
    }
}

TEST(tile_selection, wang_sites) {
    for (const auto type : {CE::Assets::Tilesets::WangType::Edge, CE::Assets::Tilesets::WangType::Corner}) {
        auto rule = wang_rule(type);
        const CE::Assets::Tilesets::WangSignature signature = type == CE::Assets::Tilesets::WangType::Edge
                                                                  ? CE::Assets::Tilesets::WangSignature{1, 0, 2, 0, 1, 0, 2, 0}
                                                                  : CE::Assets::Tilesets::WangSignature{0, 1, 0, 2, 0, 1, 0, 2};
        add_variant(rule, 7, signature);
        std::vector<CE::Assets::Tilesets::Selection::Site> calls;
        const CE::Assets::Tilesets::Selection::Sampler sampler = [&](const CE::Assets::Tilesets::Selection::Site site) {
            calls.push_back(site);
            const bool vertical =
                site.direction == CE::Assets::Tilesets::Direction::North || site.direction == CE::Assets::Tilesets::Direction::NorthEast ||
                site.direction == CE::Assets::Tilesets::Direction::South || site.direction == CE::Assets::Tilesets::Direction::SouthWest;
            return CE::Assets::Tilesets::Selection::Sample{CE::Assets::Tilesets::Selection::SampleState::Known, vertical ? 1u : 2u};
        };
        EXPECT_EQ(
            CE::Assets::Tilesets::Selection::select(rule, sampler, {}),
            (CE::Assets::Tilesets::Selection::Result{CE::Assets::Grids::CellIndex{7}})
        );
        const std::vector<CE::Assets::Tilesets::Selection::Site> expected = type == CE::Assets::Tilesets::WangType::Edge
                                                                                ? std::vector<
                                                                                      CE::Assets::Tilesets::
                                                                                          Selection::Site>{{CE::Assets::Tilesets::Selection::SiteKind::Edge, CE::Assets::Tilesets::Direction::North}, {CE::Assets::Tilesets::Selection::SiteKind::Edge, CE::Assets::Tilesets::Direction::East}, {CE::Assets::Tilesets::Selection::SiteKind::Edge, CE::Assets::Tilesets::Direction::South}, {CE::Assets::Tilesets::Selection::SiteKind::Edge, CE::Assets::Tilesets::Direction::West}}
                                                                                : std::vector<CE::Assets::Tilesets::Selection::Site>{
                                                                                      {CE::Assets::Tilesets::Selection::SiteKind::Corner,
                                                                                       CE::Assets::Tilesets::Direction::NorthEast},
                                                                                      {CE::Assets::Tilesets::Selection::SiteKind::Corner,
                                                                                       CE::Assets::Tilesets::Direction::SouthEast},
                                                                                      {CE::Assets::Tilesets::Selection::SiteKind::Corner,
                                                                                       CE::Assets::Tilesets::Direction::SouthWest},
                                                                                      {CE::Assets::Tilesets::Selection::SiteKind::Corner,
                                                                                       CE::Assets::Tilesets::Direction::NorthWest}
                                                                                  };
        EXPECT_EQ(calls, expected);
        const CE::Assets::Tilesets::Rule definition = rule;
        EXPECT_EQ(
            CE::Assets::Tilesets::Selection::select(definition, sampler, {}),
            (CE::Assets::Tilesets::Selection::Result{CE::Assets::Grids::CellIndex{7}})
        );
    }
}

TEST(tile_selection, bit_order) {
    const auto rule = bit_rule();
    std::vector<CE::Assets::Tilesets::Selection::Site> calls;
    const CE::Assets::Tilesets::Selection::Sampler sampler = [&](const CE::Assets::Tilesets::Selection::Site site) {
        calls.push_back(site);
        if (site.direction == CE::Assets::Tilesets::Direction::North || site.direction == CE::Assets::Tilesets::Direction::East)
            return CE::Assets::Tilesets::Selection::Sample{CE::Assets::Tilesets::Selection::SampleState::Known, 1};
        return CE::Assets::Tilesets::Selection::Sample{
            CE::Assets::Tilesets::Selection::SampleState::Known, site.direction == CE::Assets::Tilesets::Direction::South ? 2u : 0u
        };
    };
    const CE::Assets::Tilesets::Selection::Options options{.terrain = 1};
    EXPECT_EQ(
        CE::Assets::Tilesets::Selection::select(rule, sampler, options),
        (CE::Assets::Tilesets::Selection::Result{CE::Assets::Grids::CellIndex{13}})
    );
    const std::vector<CE::Assets::Tilesets::Selection::Site> expected{
        {CE::Assets::Tilesets::Selection::SiteKind::Cell, CE::Assets::Tilesets::Direction::North},
        {CE::Assets::Tilesets::Selection::SiteKind::Cell, CE::Assets::Tilesets::Direction::East},
        {CE::Assets::Tilesets::Selection::SiteKind::Cell, CE::Assets::Tilesets::Direction::South},
        {CE::Assets::Tilesets::Selection::SiteKind::Cell, CE::Assets::Tilesets::Direction::West}
    };
    EXPECT_EQ(calls, expected);
    const CE::Assets::Tilesets::Rule definition = rule;
    EXPECT_EQ(
        CE::Assets::Tilesets::Selection::select(definition, sampler, options),
        (CE::Assets::Tilesets::Selection::Result{CE::Assets::Grids::CellIndex{13}})
    );
    EXPECT_EQ(
        CE::Assets::Tilesets::Selection::select(rule, uniform(0), {}),
        (CE::Assets::Tilesets::Selection::Result{CE::Assets::Grids::CellIndex{3}})
    ); // Empty never connects to empty.
}

TEST(tile_selection, diagonals) {
    CE::Assets::Tilesets::BitmaskRule rule;
    rule.type = CE::Assets::Tilesets::BitmaskType::EightNeighbor;
    rule.bit_order = {CE::Assets::Tilesets::Direction::NorthEast};
    rule.cases = {{0, 3}, {1, 4}};
    std::vector<CE::Assets::Tilesets::Selection::Site> calls;
    const CE::Assets::Tilesets::Selection::Sampler sampler = [&](const CE::Assets::Tilesets::Selection::Site site) {
        calls.push_back(site);
        return CE::Assets::Tilesets::Selection::Sample{
            CE::Assets::Tilesets::Selection::SampleState::Known, site.direction == CE::Assets::Tilesets::Direction::East ? 2u : 1u
        };
    };
    CE::Assets::Tilesets::Selection::Options options{.terrain = 1};
    EXPECT_EQ(
        CE::Assets::Tilesets::Selection::select(rule, sampler, options),
        (CE::Assets::Tilesets::Selection::Result{CE::Assets::Grids::CellIndex{3}})
    );
    const std::vector<CE::Assets::Tilesets::Selection::Site> gated{
        {CE::Assets::Tilesets::Selection::SiteKind::Cell, CE::Assets::Tilesets::Direction::North},
        {CE::Assets::Tilesets::Selection::SiteKind::Cell, CE::Assets::Tilesets::Direction::NorthEast},
        {CE::Assets::Tilesets::Selection::SiteKind::Cell, CE::Assets::Tilesets::Direction::East}
    };
    EXPECT_EQ(calls, gated);
    EXPECT_EQ(
        CE::Assets::Tilesets::Selection::select(rule, uniform(1), options),
        (CE::Assets::Tilesets::Selection::Result{CE::Assets::Grids::CellIndex{4}})
    );
    options.diagonals = CE::Assets::Tilesets::Selection::DiagonalConnectivity::Independent;
    calls.clear();
    EXPECT_EQ(
        CE::Assets::Tilesets::Selection::select(rule, sampler, options),
        (CE::Assets::Tilesets::Selection::Result{CE::Assets::Grids::CellIndex{4}})
    );
    EXPECT_EQ(
        calls, (std::vector<CE::Assets::Tilesets::Selection::Site>{
                   {CE::Assets::Tilesets::Selection::SiteKind::Cell, CE::Assets::Tilesets::Direction::NorthEast}
               })
    );

    const CE::Assets::Tilesets::Selection::Sampler unknown_side = [](const CE::Assets::Tilesets::Selection::Site site) {
        if (site.direction == CE::Assets::Tilesets::Direction::North)
            return CE::Assets::Tilesets::Selection::Sample{};
        return CE::Assets::Tilesets::Selection::Sample{CE::Assets::Tilesets::Selection::SampleState::Known, 0};
    };
    EXPECT_EQ(
        CE::Assets::Tilesets::Selection::select(rule, unknown_side, options),
        (CE::Assets::Tilesets::Selection::Result{CE::Assets::Grids::CellIndex{3}})
    );
    options.diagonals = CE::Assets::Tilesets::Selection::DiagonalConnectivity::RequireCardinals;
    EXPECT_EQ(
        CE::Assets::Tilesets::Selection::select(rule, unknown_side, options),
        (CE::Assets::Tilesets::Selection::Result{CE::Assets::Tilesets::Selection::Failure::IncompleteNeighborhood})
    );

    rule.bit_order = {CE::Assets::Tilesets::Direction::North, CE::Assets::Tilesets::Direction::NorthEast,
                      CE::Assets::Tilesets::Direction::East,  CE::Assets::Tilesets::Direction::SouthEast,
                      CE::Assets::Tilesets::Direction::South, CE::Assets::Tilesets::Direction::SouthWest,
                      CE::Assets::Tilesets::Direction::West,  CE::Assets::Tilesets::Direction::NorthWest};
    rule.cases.emplace(255, 8);
    options.diagonals = CE::Assets::Tilesets::Selection::DiagonalConnectivity::RequireCardinals;
    EXPECT_EQ(
        CE::Assets::Tilesets::Selection::select(rule, uniform(1), options),
        (CE::Assets::Tilesets::Selection::Result{CE::Assets::Grids::CellIndex{8}})
    );
}

TEST(tile_selection, boundaries) {
    auto rule = bit_rule();
    rule.bit_order = {CE::Assets::Tilesets::Direction::North};
    rule.cases = {{0, 3}, {1, 4}};
    CE::Assets::Tilesets::Selection::Options options{.terrain = 1};
    CE::Assets::Tilesets::Selection::Sampler sampler = [](CE::Assets::Tilesets::Selection::Site) {
        return CE::Assets::Tilesets::Selection::Sample{CE::Assets::Tilesets::Selection::SampleState::Outside, 99};
    };
    EXPECT_EQ(
        CE::Assets::Tilesets::Selection::select(rule, sampler, options),
        (CE::Assets::Tilesets::Selection::Result{CE::Assets::Grids::CellIndex{3}})
    );
    options.outside = CE::Assets::Tilesets::Selection::Fallback::Center;
    EXPECT_EQ(
        CE::Assets::Tilesets::Selection::select(rule, sampler, options),
        (CE::Assets::Tilesets::Selection::Result{CE::Assets::Grids::CellIndex{4}})
    );
    options.outside = CE::Assets::Tilesets::Selection::Fallback::Unresolved;
    EXPECT_EQ(
        CE::Assets::Tilesets::Selection::select(rule, sampler, options),
        (CE::Assets::Tilesets::Selection::Result{CE::Assets::Tilesets::Selection::Failure::IncompleteNeighborhood})
    );

    sampler = [](CE::Assets::Tilesets::Selection::Site) { return CE::Assets::Tilesets::Selection::Sample{}; };
    EXPECT_EQ(
        CE::Assets::Tilesets::Selection::select(rule, sampler, options),
        (CE::Assets::Tilesets::Selection::Result{CE::Assets::Tilesets::Selection::Failure::IncompleteNeighborhood})
    );
    options.unknown = CE::Assets::Tilesets::Selection::Fallback::Empty;
    EXPECT_EQ(
        CE::Assets::Tilesets::Selection::select(rule, sampler, options),
        (CE::Assets::Tilesets::Selection::Result{CE::Assets::Grids::CellIndex{3}})
    );
    options.unknown = CE::Assets::Tilesets::Selection::Fallback::Center;
    EXPECT_EQ(
        CE::Assets::Tilesets::Selection::select(rule, sampler, options),
        (CE::Assets::Tilesets::Selection::Result{CE::Assets::Grids::CellIndex{4}})
    );
}

TEST(tile_selection, unknown_wang) {
    auto rule = wang_rule();
    add_variant(rule, 3, {});
    add_variant(rule, 4, {1, 0, 1, 0, 1, 0, 1, 0});
    CE::Assets::Tilesets::Selection::Options options{.terrain = 1};
    EXPECT_EQ(
        CE::Assets::Tilesets::Selection::select(rule, uniform(0), options),
        (CE::Assets::Tilesets::Selection::Result{CE::Assets::Grids::CellIndex{3}})
    );
    EXPECT_EQ(
        CE::Assets::Tilesets::Selection::select(rule, uniform(99), options),
        (CE::Assets::Tilesets::Selection::Result{CE::Assets::Tilesets::Selection::Failure::IncompleteNeighborhood})
    );
    options.unknown = CE::Assets::Tilesets::Selection::Fallback::Empty;
    EXPECT_EQ(
        CE::Assets::Tilesets::Selection::select(rule, uniform(99), options),
        (CE::Assets::Tilesets::Selection::Result{CE::Assets::Grids::CellIndex{3}})
    );
    options.unknown = CE::Assets::Tilesets::Selection::Fallback::Center;
    EXPECT_EQ(
        CE::Assets::Tilesets::Selection::select(rule, uniform(99), options),
        (CE::Assets::Tilesets::Selection::Result{CE::Assets::Grids::CellIndex{4}})
    );
    options.terrain = 99;
    EXPECT_THROW(static_cast<void>(CE::Assets::Tilesets::Selection::select(rule, uniform(1), options)), CE::Exceptions::invalid_args);
}

TEST(tile_selection, missing) {
    auto bitmask = bit_rule();
    EXPECT_EQ(
        CE::Assets::Tilesets::Selection::select(bitmask, uniform(2), {.terrain = 2}),
        (CE::Assets::Tilesets::Selection::Result{CE::Assets::Grids::CellIndex{20}})
    );
    bitmask.cases.erase(15);
    EXPECT_EQ(
        CE::Assets::Tilesets::Selection::select(bitmask, uniform(2), {.terrain = 2}),
        (CE::Assets::Tilesets::Selection::Result{CE::Assets::Tilesets::Selection::Failure::MissingRule})
    );
    auto wang = wang_rule();
    EXPECT_EQ(
        CE::Assets::Tilesets::Selection::select(wang, uniform(1), {}),
        (CE::Assets::Tilesets::Selection::Result{CE::Assets::Tilesets::Selection::Failure::MissingRule})
    );
    wang.variants[{1, 0, 1, 0, 1, 0, 1, 0}] = {};
    EXPECT_EQ(
        CE::Assets::Tilesets::Selection::select(wang, uniform(1), {}),
        (CE::Assets::Tilesets::Selection::Result{CE::Assets::Tilesets::Selection::Failure::MissingRule})
    );

    std::size_t calls = 0;
    const CE::Assets::Tilesets::Selection::Sampler unavailable = [&](CE::Assets::Tilesets::Selection::Site) {
        ++calls;
        return CE::Assets::Tilesets::Selection::Sample{};
    };
    EXPECT_EQ(
        CE::Assets::Tilesets::Selection::select(bitmask, unavailable, {}),
        (CE::Assets::Tilesets::Selection::Result{CE::Assets::Tilesets::Selection::Failure::IncompleteNeighborhood})
    );
    EXPECT_EQ(calls, 4u); // An unresolved sample does not skip later required sites.
    EXPECT_THROW(
        static_cast<void>(CE::Assets::Tilesets::Selection::select(
            bitmask,
            [](CE::Assets::Tilesets::Selection::Site) -> CE::Assets::Tilesets::Selection::Sample { throw std::runtime_error("sample"); }, {}
        )),
        std::runtime_error
    );
}

TEST(tile_selection, weights) {
    auto rule = wang_rule();
    const CE::Assets::Tilesets::WangSignature signature{1, 0, 1, 0, 1, 0, 1, 0};
    add_variant(rule, 4, signature, 1.0);
    add_variant(rule, 9, signature, 3.0);
    std::array<CE::Assets::Grids::CellIndex, 256> cells{};
    std::size_t heavy = 0;
    for (std::size_t seed = 0; seed < cells.size(); ++seed) {
        cells[seed] = std::get<CE::Assets::Grids::CellIndex>(CE::Assets::Tilesets::Selection::select(rule, uniform(1), {.seed = seed}));
        EXPECT_TRUE(cells[seed] == 4 || cells[seed] == 9);
        if (cells[seed] == 9)
            ++heavy;
    }
    EXPECT_GT(heavy, 150u);
    EXPECT_LT(heavy, 230u);
    for (std::size_t index = cells.size(); index > 0; --index) {
        const auto seed = index - 1;
        EXPECT_EQ(
            CE::Assets::Tilesets::Selection::select(rule, uniform(1), {.seed = seed}),
            (CE::Assets::Tilesets::Selection::Result{cells[seed]})
        );
    }
    rule.tiles[0].weight = 2.0;
    rule.tiles[1].weight = 6.0;
    for (std::size_t seed = 0; seed < cells.size(); ++seed)
        EXPECT_EQ(
            CE::Assets::Tilesets::Selection::select(rule, uniform(1), {.seed = seed}),
            (CE::Assets::Tilesets::Selection::Result{cells[seed]})
        );

    rule.tiles[0].weight = std::numeric_limits<double>::max();
    rule.tiles[1].weight = std::numeric_limits<double>::max();
    const auto wide = rule;
    rule.tiles[0].weight = 1.0;
    rule.tiles[1].weight = 1.0;
    for (std::uint64_t seed = 0; seed < 32; ++seed)
        EXPECT_EQ(
            CE::Assets::Tilesets::Selection::select(wide, uniform(1), {.seed = seed}),
            CE::Assets::Tilesets::Selection::select(rule, uniform(1), {.seed = seed})
        );
}

TEST(tile_selection, invalid_bitmask) {
    auto rule = bit_rule();
    EXPECT_THROW(static_cast<void>(CE::Assets::Tilesets::Selection::select(rule, {}, {})), CE::Exceptions::invalid_args);
    EXPECT_THROW(
        static_cast<void>(CE::Assets::Tilesets::Selection::select(
            rule, uniform(1), {.unknown = static_cast<CE::Assets::Tilesets::Selection::Fallback>(99)}
        )),
        CE::Exceptions::invalid_args
    );
    EXPECT_THROW(
        static_cast<void>(CE::Assets::Tilesets::Selection::select(
            rule, uniform(1), {.diagonals = static_cast<CE::Assets::Tilesets::Selection::DiagonalConnectivity>(99)}
        )),
        CE::Exceptions::invalid_args
    );
    const CE::Assets::Tilesets::Selection::Sampler invalid = [](CE::Assets::Tilesets::Selection::Site) {
        return CE::Assets::Tilesets::Selection::Sample{static_cast<CE::Assets::Tilesets::Selection::SampleState>(99), 0};
    };
    EXPECT_THROW(static_cast<void>(CE::Assets::Tilesets::Selection::select(rule, invalid, {})), CE::Exceptions::invalid_args);
    rule.bit_order = {CE::Assets::Tilesets::Direction::North, CE::Assets::Tilesets::Direction::North};
    EXPECT_THROW(static_cast<void>(CE::Assets::Tilesets::Selection::select(rule, uniform(1), {})), CE::Exceptions::invalid_args);
    rule.bit_order = {CE::Assets::Tilesets::Direction::NorthEast};
    EXPECT_THROW(static_cast<void>(CE::Assets::Tilesets::Selection::select(rule, uniform(1), {})), CE::Exceptions::invalid_args);
    rule.bit_order = {static_cast<CE::Assets::Tilesets::Direction>(99)};
    EXPECT_THROW(static_cast<void>(CE::Assets::Tilesets::Selection::select(rule, uniform(1), {})), CE::Exceptions::invalid_args);
    rule.bit_order.clear();
    EXPECT_THROW(static_cast<void>(CE::Assets::Tilesets::Selection::select(rule, uniform(1), {})), CE::Exceptions::invalid_args);
}

TEST(tile_selection, invalid_wang) {
    auto rule = wang_rule();
    const CE::Assets::Tilesets::WangSignature signature{1, 0, 1, 0, 1, 0, 1, 0};
    add_variant(rule, 4, signature);
    add_variant(rule, 9, signature);
    for (const auto weight : {0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        rule.tiles[1].weight = weight;
        EXPECT_THROW(static_cast<void>(CE::Assets::Tilesets::Selection::select(rule, uniform(1), {})), CE::Exceptions::invalid_args);
    }
    rule.tiles[1].weight = 1.0;
    rule.tiles[0].wang[0] = 2;
    EXPECT_THROW(static_cast<void>(CE::Assets::Tilesets::Selection::select(rule, uniform(1), {})), CE::Exceptions::invalid_args);
    rule.tiles[0].wang = signature;
    rule.variants[signature] = {2};
    EXPECT_THROW(static_cast<void>(CE::Assets::Tilesets::Selection::select(rule, uniform(1), {})), CE::Exceptions::invalid_args);
    rule.variants[signature] = {0, 0};
    EXPECT_THROW(static_cast<void>(CE::Assets::Tilesets::Selection::select(rule, uniform(1), {})), CE::Exceptions::invalid_args);
    rule.variants[signature] = {1, 0};
    EXPECT_THROW(static_cast<void>(CE::Assets::Tilesets::Selection::select(rule, uniform(1), {})), CE::Exceptions::invalid_args);
    rule.variants[signature] = {0};
    rule.slot_order[0] = CE::Assets::Tilesets::Direction::South;
    EXPECT_THROW(static_cast<void>(CE::Assets::Tilesets::Selection::select(rule, uniform(1), {})), CE::Exceptions::invalid_args);
}

TEST(tile_selection, manifest_order) {
    const auto parse = [](const std::string& type, const std::string& order) {
        std::istringstream input(R"json({"$schema":"../schemas/graphics/sprite-tileset-manifest-2.0.schema.json",
          "asset_class":"sprite-tileset","version":"2.0","namespace":"test","texture":"sheet.png",
          "defaults":{"sprite":{"pivot":{"x":0.5,"y":0.5}},"tileset":{"pivot":{"x":0.5,"y":0.5}}},
          "tilesets":{"terrain":{"grid":{"origin":{"x":0,"y":0},"frame":{"width":16,"height":16},
          "spacing":{"x":0,"y":0},"rows":1,"columns":1,"cell_order":"row-major"},
          "autotiles":{"rule":{"type":")json" + type + R"json(","bit_order":)json" + order +
            R"json(,"cases":{"0":{"index":0}}}}}}})json");
        return CE::Assets::Manifests::Reader::parse(input, "graphics/bitmask.json");
    };
    EXPECT_THROW(static_cast<void>(parse("four-neighbor", R"(["north_east"])")), CE::Exceptions::runtime_exception);
    EXPECT_THROW(static_cast<void>(parse("four-neighbor", R"(["north","east","south","west","north_east"])")),
        CE::Exceptions::runtime_exception);
    const auto manifest = parse("eight-neighbor", R"(["north_east"])");
    const auto& rule = std::get<CE::Assets::Tilesets::BitmaskRule>(manifest.tilesets.front().autotiles.at("rule"));
    EXPECT_EQ(rule.bit_order, (std::vector{CE::Assets::Tilesets::Direction::NorthEast}));
}
