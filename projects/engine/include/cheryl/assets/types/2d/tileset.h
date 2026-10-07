#pragma once

#include <assets/types/2d/base/asset2d.h>
#include <assets/types/primitives/frame.h>
#include <assets/definitions/tileset.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <cstddef>

namespace CE::Assets {
    template <typename T> using shptr = std::shared_ptr<T>;

    struct TilesetData {
        shptr<Geometry2D> geometry;
        shptr<Image> texture;
        TilesetDefinition definition;
    };

    struct Tile final : Asset2D,
                        Frame<Tile> {
        explicit Tile(std::size_t cell, const shptr<Geometry2D>& geometry, const shptr<Image>& texture)
        : Asset2D(geometry, texture), Frame(cell, 0, 1) {}

        [[nodiscard]] std::size_t cell() const { return offset_; }
    };

    /** Owns a clip copy and retains its resources. Frame's index selects a clip entry,
     * not a grid cell; looping wraps and nonlooping clamps. An empty clip throws.
     * The caller schedules index changes from millisecond durations on one owner.
     */
    struct TileAnimation final : Asset2D,
                                 Frame<TileAnimation> {
    private:
        TileAnimationDefinition definition_;

    public:
        explicit TileAnimation(TileAnimationDefinition definition, const shptr<Geometry2D>& geometry, const shptr<Image>& texture);

        [[nodiscard]] const TileAnimationDefinition& definition() const { return definition_; }
        [[nodiscard]] std::chrono::milliseconds frame_duration() const;
        [[nodiscard]] bool loops() const { return definition_.loop; }
    };

    // TODO: Integrate a tile-map selection layer here: derive a Wang signature or bitmask from
    // neighboring terrain, choose a weighted candidate, then substitute animation_for(target)
    // using simulation-owned elapsed time before submitting the resolved tile to rendering.
    /** Shared tile grid plus definitions for static cells, animated targets, and autotile rules.
     * These queries expose metadata; they do not inspect a world or choose neighbors.
     * Direct construction rejects duplicate targets but does not revalidate all manifest
     * fields. Keep definitions/resources stable for concurrent read-only queries.
     */
    struct Tileset final : Asset2D {
    private:
        TilesetDefinition definition_;
        std::unordered_map<CellIndex, std::string> animation_targets_;

    public:
        explicit Tileset(TilesetData data);

        // Retained resource values; tile() rejects out-of-grid cells. animation() throws
        // out_of_range for an unknown name; animation_for() returns no value for no target.
        [[nodiscard]] Tile tile(std::size_t cell) const;
        [[nodiscard]] TileAnimation animation(const std::string& name) const;
        [[nodiscard]] std::optional<TileAnimation> animation_for(std::size_t target) const;
        // References borrow this Tileset's lifetime; unknown metadata names throw out_of_range.
        [[nodiscard]] const ViewDefinition& view(const std::string& name) const;
        [[nodiscard]] CellIndex orientation(const std::string& name) const;
        [[nodiscard]] const AutotileDefinition& autotile(const std::string& name) const;
        [[nodiscard]] const TilesetDefinition& definition() const { return definition_; }
    };
}
