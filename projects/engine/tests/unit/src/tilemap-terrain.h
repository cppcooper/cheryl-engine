#pragma once

#include <assets/selection/tile-selection.h>
#include <maps/tilemap.h>

#include <optional>
#include <span>
#include <vector>

namespace TilemapTerrain {
    // Private consumer prototype, not a public Engine terrain model. Zero is
    // empty; equality of these labels, never visual IDs, determines connectivity.
    struct TerrainLabel {
        std::uint32_t value{};
        bool operator==(const TerrainLabel&) const = default;
    };

    class TerrainMap final {
        struct Change {
            CE::Tilemaps::Point point;
            TerrainLabel label;
        };

        CE::Tilemaps::Map tiles_;
        CE::Tilemaps::LayerIndex layer_;
        std::vector<TerrainLabel> labels_;
        CE::Assets::Tilesets::BitmaskRule rule_;
        std::vector<CE::Tilemaps::TileId> target_to_id_;

        [[nodiscard]] bool contains(CE::Tilemaps::Point point) const;
        [[nodiscard]] std::size_t index(CE::Tilemaps::Point point) const;
        [[nodiscard]] CE::Tilemaps::TileId resolve(CE::Tilemaps::Point point, std::optional<Change> change) const;

    public:
        // Copy all inputs; derive only the selected visual layer. Row-major
        // labels use X-right/Y-down cells. Outside samples substitute empty.
        // Rule must declare 1..4 unique cardinal bits in its authored order.
        // Mapping indices define the target domain; a zero entry is unmapped.
        // Missing cases/targets are rejected when used by a nonempty center.
        TerrainMap(
            const CE::Tilemaps::Map& base,
            CE::Tilemaps::LayerIndex layer,
            std::span<const TerrainLabel> labels,
            const CE::Assets::Tilesets::Rule& rule,
            std::span<const CE::Tilemaps::TileId> target_to_id
        );
        TerrainMap(const TerrainMap&) = delete;
        TerrainMap& operator=(const TerrainMap&) = delete;

        [[nodiscard]] TerrainLabel label(CE::Tilemaps::Point point) const;
        [[nodiscard]] const CE::Tilemaps::Map& tiles() const { return tiles_; }
        // Return owned invalidations: center, then in-bounds N/E/S/W, unique.
        // Same label is a no-op. Resolve every affected cell before changing
        // labels/tiles; failure preserves both. No references to calls survive.
        // Serialize edits, queries and preparation; tiles() borrows this owner.
        [[nodiscard]] std::vector<CE::Tilemaps::Point> edit(CE::Tilemaps::Point point, TerrainLabel label);
    };
}
