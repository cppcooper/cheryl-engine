#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace MapFixture {
    // Private CPU values. Cells use semantic tile keys, independent of installed assets.
    struct Layer {
        std::string id;
        std::vector<std::optional<std::string>> cells;

        [[nodiscard]] bool operator==(const Layer&) const = default;
    };

    struct Spawn {
        std::string name;
        std::string layer;
        std::uint32_t x = 0;
        std::uint32_t y = 0;

        [[nodiscard]] bool operator==(const Spawn&) const = default;
    };

    // Zero-origin, +x right, +y down; each ordered layer uses y * width + x.
    // Aggregate construction does not validate. parse() and encode() validate completely.
    struct Snapshot {
        std::string id;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::vector<Layer> layers;
        std::vector<Spawn> spawns;

        [[nodiscard]] bool operator==(const Snapshot&) const = default;
    };
}
