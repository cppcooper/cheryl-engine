#pragma once

#include <assets/abstracts.h>
#include <assets/manifest.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace CE::Assets {
    template <typename T>
    using shptr = std::shared_ptr<T>;

    struct TilesetData {
        shptr<Geometry2D> geometry;
        shptr<Image> texture;
        TilesetDefinition definition;
    };

    struct Tile final : Draw2D, Frame<Tile> {
        explicit Tile(std::size_t cell, const shptr<Geometry2D>& geometry, const shptr<Image>& texture) :
            Draw2D(geometry, texture), Frame(cell, 0, 1) {}

        void draw(const DrawInfo& info) override;
        [[nodiscard]] std::size_t cell() const { return offset_; }
    };

    /** A selected tile clip with mutable frame index; advancing elapsed time belongs to its caller. */
    struct TileAnimation final : Draw2D, Frame<TileAnimation> {
        explicit TileAnimation(TileAnimationDefinition definition, const shptr<Geometry2D>& geometry,
                               const shptr<Image>& texture);

        void draw(const DrawInfo& info) override;
        [[nodiscard]] const TileAnimationDefinition& definition() const { return definition_; }
        [[nodiscard]] std::chrono::milliseconds frame_duration() const;
        [[nodiscard]] bool loops() const { return definition_.loop; }

    private:
        TileAnimationDefinition definition_;
    };

    // TODO: Integrate a tile-map selection layer here: derive a Wang signature or bitmask from
    // neighboring terrain, choose a weighted candidate, then substitute animation_for(target)
    // using simulation-owned elapsed time before submitting the resolved tile to rendering.
    /** Shared tile grid plus definitions for static cells, animated targets, and autotile rules.
     * These queries expose metadata; they do not inspect a world or choose neighbors.
     */
    struct Tileset final : Asset2D, Frame<Tileset> {
        explicit Tileset(TilesetData data);
        ~Tileset() override = default;

        void draw(const DrawInfo& info) override;
        [[nodiscard]] Tile tile(std::size_t cell) const;
        [[nodiscard]] TileAnimation animation(const std::string& name) const;
        [[nodiscard]] std::optional<TileAnimation> animation_for(std::size_t target) const;
        [[nodiscard]] const ViewDefinition& view(const std::string& name) const;
        [[nodiscard]] CellIndex orientation(const std::string& name) const;
        [[nodiscard]] const AutotileDefinition& autotile(const std::string& name) const;
        [[nodiscard]] const TilesetDefinition& definition() const { return definition_; }

    private:
        TilesetDefinition definition_;
        std::unordered_map<CellIndex, std::string> animation_targets_;
    };
}
