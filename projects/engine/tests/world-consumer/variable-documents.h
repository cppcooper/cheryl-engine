#pragma once

#include <cheryl/core/resources/map-storage/snapshot.h>
#include <array>
#include <cstddef>
#include <stdexcept>

// Test-owned expected data, independent of application conversion/selection.
namespace VariableDocuments {
    namespace Storage = CE::Maps;
    constexpr std::array<std::array<std::uint32_t, 2>, 5> shapes{{{7, 5}, {1, 7}, {7, 1}, {1, 1}, {64, 64}}};
    inline const std::array<std::string, 4> literal{"tilemap:brown", "tilemap:black", "tilemap:grey", "tilemap:red"};
    inline const std::array<std::string, 4> semantic{"terrain:one", "terrain:two", "terrain:three", "terrain:four"};
    inline std::uint32_t meaning(const std::optional<std::string>& key, bool terrain) {
        if (!key)
            return 0;
        const auto& table = terrain ? semantic : literal;
        for (std::size_t i = 0; i < table.size(); ++i)
            if (*key == table[i])
                return static_cast<std::uint32_t>(i + 1);
        throw std::runtime_error("Unknown independent expected key");
    }
    inline Storage::Snapshot source(std::uint32_t width, std::uint32_t height, bool terrain, bool east = false) {
        Storage::Snapshot result{
            std::string(terrain ? "terrain:" : "literal:") + (east ? "variable-east" : "variable-west"),
            width,
            height,
            {{"floor", {}}, {"overlay", {}}},
            {{"last", "overlay", width - 1, height - 1}, {"first", "floor", 0, 0}, {"corner", "floor", width - 1, 0}}
        };
        const auto& table = terrain ? semantic : literal;
        for (std::size_t layer = 0; layer < 2; ++layer)
            for (std::uint32_t y = 0; y < height; ++y)
                for (std::uint32_t x = 0; x < width; ++x) {
                    // Repeated regions, distinct labels and explicit nulls in BOTH layers.
                    const auto value = (x / 2 + y + 2 * layer + (east ? 1u : 0u)) % 5;
                    result.layers[layer].cells.push_back(value == 4 ? std::nullopt : std::optional<std::string>(table[value]));
                }
        return result;
    }
    inline std::uint32_t numeric(const std::optional<std::string>& key, bool terrain) {
        const auto label = meaning(key, terrain);
        constexpr std::array<std::uint32_t, 5> meanings{0, 101, 203, 307, 409};
        return terrain ? meanings[label] : (label ? 6 + label : 0);
    }
    inline std::uint32_t visual(const Storage::Snapshot& source, std::size_t layer, std::uint32_t x, std::uint32_t y) {
        const auto& cells = source.layers.at(layer).cells;
        const auto label = meaning(cells.at(std::size_t(y) * source.width + x), true);
        if (!label)
            return 0;
        // Literal neighbor tests, NOT the production selector/search or incremental helper.
        unsigned mask = 0;
        if (y > 0 && meaning(cells.at(std::size_t(y - 1) * source.width + x), true) == label)
            mask |= 1;
        if (x + 1 < source.width && meaning(cells.at(std::size_t(y) * source.width + x + 1), true) == label)
            mask |= 2;
        if (y + 1 < source.height && meaning(cells.at(std::size_t(y + 1) * source.width + x), true) == label)
            mask |= 4;
        if (x > 0 && meaning(cells.at(std::size_t(y) * source.width + x - 1), true) == label)
            mask |= 8;
        constexpr std::array<std::uint32_t, 4> targets{7, 8, 9, 10};
        return targets[mask % 4];
    }
}
