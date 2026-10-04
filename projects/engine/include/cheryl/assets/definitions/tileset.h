#pragma once

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

namespace CE::Assets {
    enum class Direction { North, NorthEast, East, SouthEast, South, SouthWest, West, NorthWest };

    enum class WangType { Corner, Edge };

    struct TerrainDefinition {
        std::uint32_t id{};
        std::string name;
        std::optional<std::string> color;
        double probability{1.0};
        std::optional<CellIndex> representative;
    };

    using WangSignature = std::array<std::uint32_t, 8>;

    struct WangSignatureHash {
        [[nodiscard]] std::size_t operator()(const WangSignature& signature) const noexcept;
    };

    struct WangTileDefinition {
        CellIndex cell{};
        WangSignature wang{};
        double weight{1.0};
    };

    /** Parsed terrain signatures and candidate tile indices. A future tile-map system computes
     * a signature from neighbors, looks up variants, and selects by their stored weights.
     */
    struct WangAutotileDefinition {
        std::string description;
        WangType type{};
        std::array<Direction, 8> slot_order{};
        std::vector<TerrainDefinition> terrains;
        std::vector<WangTileDefinition> tiles;
        std::unordered_map<WangSignature, std::vector<std::size_t>, WangSignatureHash> variants;
    };

    enum class BitmaskType { FourNeighbor, EightNeighbor };

    struct BitmaskAutotileDefinition {
        std::string description;
        BitmaskType type{};
        std::vector<Direction> bit_order;
        std::unordered_map<std::uint32_t, CellIndex> cases;
    };

    using AutotileDefinition = std::variant<WangAutotileDefinition, BitmaskAutotileDefinition>;

    struct TilesetDefinition {
        std::string name_space;
        std::string name;
        std::string description;
        std::filesystem::path texture;
        GridDefinition grid;
        math::Pivot pivot;
        std::unordered_map<std::string, ViewDefinition> views;
        std::unordered_map<std::string, CellIndex> orientations;
        std::unordered_map<std::string, TileAnimationDefinition> animations;
        std::unordered_map<std::string, AutotileDefinition> autotiles;

        [[nodiscard]] std::string id() const;
    };
}
