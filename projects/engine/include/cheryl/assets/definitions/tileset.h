#pragma once

#include <cheryl/export.h>

#include <assets/definitions/animation.h>
#include <assets/definitions/grid.h>
#include <math/anchor.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace CE::Assets::Tilesets {
    enum class Direction { North, NorthEast, East, SouthEast, South, SouthWest, West, NorthWest };

    enum class WangType { Corner, Edge };

    struct Terrain {
        std::uint32_t id{};
        std::string name;
        std::optional<std::string> color;
        double probability{1.0};
        std::optional<CE::Assets::Grids::CellIndex> representative;
    };

    using WangSignature = std::array<std::uint32_t, 8>;

    struct WangSignatureHash {
        [[nodiscard]] CE_API std::size_t operator()(const WangSignature& signature) const noexcept;
    };

    struct WangTile {
        CE::Assets::Grids::CellIndex cell{};
        WangSignature wang{};
        double weight{1.0};
    };

    /** Parsed terrain signatures and candidate tile indices in definition order.
     * CPU selection samples shared edge/corner labels and chooses a weighted variant.
     */
    struct WangRule {
        std::string description;
        WangType type{};
        std::array<Direction, 8> slot_order{};
        std::vector<Terrain> terrains;
        std::vector<WangTile> tiles;
        std::unordered_map<WangSignature, std::vector<std::size_t>, WangSignatureHash> variants;
    };

    enum class BitmaskType { FourNeighbor, EightNeighbor };

    struct BitmaskRule {
        std::string description;
        BitmaskType type{};
        std::vector<Direction> bit_order;
        std::unordered_map<std::uint32_t, CE::Assets::Grids::CellIndex> cases;
    };

    using Rule = std::variant<WangRule, BitmaskRule>;

    struct Definition {
        std::string name_space;
        std::string name;
        std::string description;
        std::filesystem::path texture;
        CE::Assets::Grids::Definition grid;
        CE::Rendering::Geometry::Pivot pivot;
        std::unordered_map<std::string, CE::Assets::Grids::View> views;
        std::unordered_map<std::string, CE::Assets::Grids::CellIndex> orientations;
        std::unordered_map<std::string, Clip> animations;
        std::unordered_map<std::string, Rule> autotiles;
        std::optional<std::string> shader = {};

        [[nodiscard]] CE_API std::string id() const;
    };
}
