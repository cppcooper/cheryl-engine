#pragma once

#include <math/anchor.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace CE::Assets {
    using CellIndex = std::size_t;

    struct PixelPoint {
        std::uint32_t x{};
        std::uint32_t y{};
    };

    struct PixelSize {
        std::uint32_t width{};
        std::uint32_t height{};
    };

    struct PixelRect {
        std::uint64_t x{};
        std::uint64_t y{};
        std::uint32_t width{};
        std::uint32_t height{};
    };

    struct GridDefinition {
        PixelPoint origin;
        PixelSize frame;
        PixelPoint spacing;
        std::size_t rows{};
        std::size_t columns{};

        [[nodiscard]] std::size_t cell_count() const;
        [[nodiscard]] CellIndex cell_index(std::size_t row, std::size_t column) const;
        [[nodiscard]] PixelRect cell_rect(CellIndex cell) const;
        [[nodiscard]] std::uint64_t occupied_right() const;
        [[nodiscard]] std::uint64_t occupied_bottom() const;
    };

    struct ViewDefinition {
        std::string description;
        std::size_t row{};
        std::size_t column{};
        std::size_t rows{};
        std::size_t columns{};
    };

    struct TimedFrameDefinition {
        CellIndex cell{};
        std::chrono::milliseconds duration{};
    };

    struct SpriteAnimationDefinition {
        std::string name;
        std::optional<std::string> facing;
        std::string description;
        std::vector<TimedFrameDefinition> frames;
        bool loop{};
    };

    struct TileAnimationDefinition {
        std::string name;
        std::string description;
        CellIndex target{};
        std::vector<TimedFrameDefinition> frames;
        bool loop{};
    };

    struct ProfileClipDefinition {
        std::string description;
        std::size_t row_offset{};
        std::vector<std::size_t> columns;
        std::chrono::milliseconds frame_duration{};
        bool loop{};
    };

    struct AnimationProfileDefinition {
        std::string description;
        std::unordered_map<std::string, std::size_t> facings;
        std::unordered_map<std::string, ProfileClipDefinition> clips;
    };

    enum class Direction {
        North,
        NorthEast,
        East,
        SouthEast,
        South,
        SouthWest,
        West,
        NorthWest
    };

    enum class WangType {
        Corner,
        Edge
    };

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

    struct WangAutotileDefinition {
        std::string description;
        WangType type{};
        std::array<Direction, 8> slot_order{};
        std::vector<TerrainDefinition> terrains;
        std::vector<WangTileDefinition> tiles;
        std::unordered_map<WangSignature, std::vector<std::size_t>, WangSignatureHash> variants;
    };

    enum class BitmaskType {
        FourNeighbor,
        EightNeighbor
    };

    struct BitmaskAutotileDefinition {
        std::string description;
        BitmaskType type{};
        std::vector<Direction> bit_order;
        std::unordered_map<std::uint32_t, CellIndex> cases;
    };

    using AutotileDefinition = std::variant<WangAutotileDefinition, BitmaskAutotileDefinition>;

    struct SpriteDefinition {
        std::string name_space;
        std::string name;
        std::string description;
        std::filesystem::path texture;
        GridDefinition grid;
        math::Pivot pivot;
        std::unordered_map<std::string, ViewDefinition> views;
        std::unordered_map<std::string, CellIndex> orientations;
        std::optional<std::string> animation_profile;
        std::vector<SpriteAnimationDefinition> animations;

        [[nodiscard]] std::string id() const;
    };

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

    struct AssetManifest {
        std::filesystem::path source;
        std::string schema;
        std::string version;
        std::string name_space;
        math::Pivot default_sprite_pivot;
        math::Pivot default_tileset_pivot;
        std::optional<std::filesystem::path> texture;
        std::unordered_map<std::string, AnimationProfileDefinition> animation_profiles;
        std::vector<SpriteDefinition> sprites;
        std::vector<TilesetDefinition> tilesets;

        [[nodiscard]] std::vector<std::filesystem::path> textures() const;
    };
}
