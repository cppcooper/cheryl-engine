#include <cheryl/maps/terrain-map.h>

#include <internals/exceptions.h>

#include <array>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>

#if defined(CE_ENGINE_STATIC) || defined(CE_BUILDING_ENGINE)
#error This consumer must import the normal shared Engine, not compile as its producer.
#endif

namespace {
    namespace Maps = CE::Tilemaps;
    namespace Tiles = CE::Assets::Tilesets;
    constexpr Maps::LayerIndex ground{0}, detail{1};
    using CE::Exceptions::bad_request;
    using CE::Exceptions::invalid_args;

    static_assert(!std::is_copy_constructible_v<Maps::TerrainMap> && !std::is_move_constructible_v<Maps::TerrainMap>);
    static_assert(std::is_nothrow_copy_constructible_v<invalid_args> && std::is_nothrow_move_constructible_v<invalid_args>);
    static_assert(std::is_nothrow_copy_constructible_v<bad_request> && std::is_nothrow_move_constructible_v<bad_request>);

    void require(const bool condition, const char* message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    Tiles::BitmaskRule cardinal() {
        Tiles::BitmaskRule rule;
        rule.type = Tiles::BitmaskType::FourNeighbor;
        rule.bit_order = {Tiles::Direction::North, Tiles::Direction::East, Tiles::Direction::South, Tiles::Direction::West};
        for (std::uint32_t mask = 0; mask < 16; ++mask)
            rule.cases.emplace(mask, mask);
        return rule;
    }

    std::array<Maps::TileId, 16> mapping() {
        std::array<Maps::TileId, 16> result{};
        for (std::uint32_t mask = 0; mask < result.size(); ++mask)
            result[mask] = {100 + mask};
        return result;
    }

    // Every input owner dies before callers query, edit or destroy the result.
    std::unique_ptr<Maps::TerrainMap> owned() {
        Maps::Map base({3, 2}, 2);
        base.set(detail, {2, 1}, {999});
        std::array<Maps::TerrainLabel, 6> labels{{{1}, {1}, {1}, {1}, {1}, {1}}};
        Tiles::Rule rule = cardinal();
        auto ids = mapping();
        auto result = std::make_unique<Maps::TerrainMap>(base, ground, labels, rule, ids);
        labels.fill({0});
        ids.fill({0});
        std::get<Tiles::BitmaskRule>(rule).cases.clear();
        base.set(detail, {2, 1}, {0});
        return result;
    }

    void values() {
        auto terrain = owned();
        auto other = owned();
        const auto old = terrain->tiles();
        const std::array<std::uint32_t, 6> initial{106, 114, 112, 103, 111, 109};
        const std::array<std::uint32_t, 6> edited{104, 100, 104, 103, 110, 109};
        const auto affected = terrain->edit({1, 0}, {2});
        require(affected == std::vector<Maps::Point>{{1, 0}, {2, 0}, {1, 1}, {0, 0}}, "edit order changed");
        for (std::int64_t row = 0; row < 2; ++row) {
            for (std::int64_t column = 0; column < 3; ++column) {
                const Maps::Point point{column, row};
                const auto index = static_cast<std::size_t>(row * 3 + column);
                require(old.get(ground, point).value == initial[index], "old copied tiles changed");
                require(other->tiles().get(ground, point).value == initial[index], "second owner changed");
                require(terrain->tiles().get(ground, point).value == edited[index], "literal cardinal result differs");
                require(other->label(point).value == 1, "second semantic grid changed");
            }
        }
        require(terrain->label({1, 0}).value == 2, "semantic edit missing");
        require(terrain->tiles().get(detail, {2, 1}).value == 999, "copied other layer changed");
        require(terrain->edit({1, 0}, {2}).empty(), "same label must be a no-op");
        require(!terrain->edit({1, 0}, {0}).empty(), "erase failed");
        require(terrain->tiles().get(ground, {1, 0}).value == 0, "empty terrain must clear visual tile");
    }

    void errors() {
        std::exception_ptr retained;
        {
            Maps::Map base({1, 1}, 1);
            const std::array<Maps::TerrainLabel, 1> labels{};
            auto rule = cardinal();
            rule.bit_order = {Tiles::Direction::North, Tiles::Direction::North};
            try {
                Maps::TerrainMap invalid(base, ground, labels, rule, mapping());
                require(false, "invalid metadata accepted");
            } catch (const std::exception& error) {
                require(typeid(error) == typeid(invalid_args), "Engine invalid_args RTTI differs");
                require(dynamic_cast<const invalid_args*>(&error) != nullptr, "invalid_args cast failed across Engine");
                retained = std::current_exception();
            }
        }
        require(retained != nullptr, "no owned construction error");
        try {
            std::rethrow_exception(retained);
        } catch (const invalid_args& error) {
            require(
                std::string_view(error.what()).find("directions must be unique") != std::string_view::npos,
                "diagnostic lost owned metadata error"
            );
            invalid_args copy(error);
            const invalid_args moved(std::move(copy));
            require(std::string_view(moved.what()) == error.what(), "diagnostic copy/move changed message");
        }

        auto terrain = owned();
        try {
            static_cast<void>(terrain->edit({std::numeric_limits<std::int64_t>::min(), 0}, {2}));
            require(false, "invalid coordinate accepted");
        } catch (const std::exception& error) {
            require(typeid(error) == typeid(bad_request), "Engine bad_request RTTI differs");
            require(dynamic_cast<const bad_request*>(&error) != nullptr, "bad_request cast failed across Engine");
            const bad_request copy(*dynamic_cast<const bad_request*>(&error));
            require(std::string_view(copy.what()).find("out of bounds") != std::string_view::npos, "diagnostic lost owned bounds error");
        }
        require(terrain->label({0, 0}).value == 1 && terrain->tiles().get(ground, {0, 0}).value == 106, "invalid edit mutated owner");
    }

    void refusal() {
        Maps::Map base({3, 2}, 2);
        base.set(detail, {0, 0}, {999});
        const std::array<Maps::TerrainLabel, 6> labels{{{1}, {1}, {1}, {1}, {1}, {1}}};
        auto rule = cardinal();
        rule.cases.erase(2); // Initial masks exist; erase resolves center/east before south needs mask 2.
        Maps::TerrainMap terrain(base, ground, labels, rule, mapping());
        const auto before = terrain.tiles();
        bool rejected = false;
        try {
            static_cast<void>(terrain.edit({0, 0}, {0}));
        } catch (const bad_request& error) {
            rejected = true;
            require(std::string_view(error.what()).find("(0,1)") != std::string_view::npos, "wrong refusal point");
        }
        require(rejected, "late missing case accepted");
        for (std::int64_t row = 0; row < 2; ++row)
            for (std::int64_t column = 0; column < 3; ++column) {
                const Maps::Point point{column, row};
                require(terrain.label(point).value == 1, "late refusal changed semantics");
                require(terrain.tiles().get(ground, point) == before.get(ground, point), "late refusal changed visual tiles");
            }
        require(terrain.tiles().get(detail, {0, 0}).value == 999, "late refusal changed other layer");
        require(base.get(ground, {0, 0}).value == 0, "constructor mutated input map");
    }
}

int main(const int argc, const char* const* argv) {
    try {
        require(argc == 2, "expected one check name");
        const std::string_view check(argv[1]);
        if (check == "values")
            values();
        else if (check == "errors")
            errors();
        else if (check == "refusal")
            refusal();
        else
            require(false, "unknown check name");
        std::cout << check << ": passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
