#pragma once

#include <assets/types/2d/base/asset2d.h>
#include <core/resources/memory/shared.h>
#include <assets/types/primitives/frame.h>
#include <assets/definitions/tileset.h>
#include <assets/selection/tile-selection.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <cstddef>

namespace CE::Assets::Tilesets {
    struct Data {
        CE::Memory::Shared<CE::Rendering::Geometry::Buffer2D> geometry;
        CE::Memory::Shared<CE::Rendering::Resources::Image> texture;
        Definition definition;
        std::shared_ptr<const CE::Rendering::Resources::Material> material = {};
    };

    struct Tile final : Content2D,
                        CE::Assets::Animation::Cursor<Tile> {
        explicit Tile(
            std::size_t cell,
            const CE::Memory::Shared<CE::Rendering::Geometry::Buffer2D>& geometry,
            const CE::Memory::Shared<CE::Rendering::Resources::Image>& texture,
            std::shared_ptr<const CE::Rendering::Resources::Material> material = {}
        )
        : Content2D(geometry, texture, std::move(material)), CE::Assets::Animation::Cursor<Tile>(cell, 0, 1) {}

        [[nodiscard]] std::size_t cell() const { return offset_; }
    };

    /** Owns a clip copy and retains its resources. CE::Assets::Animation::Cursor's index selects a clip entry,
     * not a grid cell; looping wraps and nonlooping clamps. An empty clip throws.
     * The caller schedules index changes from millisecond durations on one owner.
     */
    struct Animation final : Content2D,
                             CE::Assets::Animation::Cursor<Animation> {
    private:
        Clip definition_;

    public:
        explicit Animation(
            Clip definition,
            const CE::Memory::Shared<CE::Rendering::Geometry::Buffer2D>& geometry,
            const CE::Memory::Shared<CE::Rendering::Resources::Image>& texture,
            std::shared_ptr<const CE::Rendering::Resources::Material> material = {}
        );

        [[nodiscard]] const Clip& definition() const { return definition_; }
        [[nodiscard]] std::chrono::milliseconds frame_duration() const;
        [[nodiscard]] bool loops() const { return definition_.loop; }
    };

    /** Shared tile grid plus definitions for static cells, animated targets, and autotile rules.
     * Selection samples through the caller's callback and resolves simulation-owned time.
     * Direct construction rejects duplicate targets but does not revalidate all manifest
     * fields. Keep definitions/resources stable for concurrent read-only queries.
     */
    struct Asset final : Content2D {
    private:
        Definition definition_;
        std::unordered_map<CE::Assets::Grids::CellIndex, std::string> animation_targets_;

    public:
        explicit Asset(Data data);

        // Retained resource values; tile() rejects out-of-grid cells. animation() throws
        // out_of_range for an unknown name; animation_for() returns no value for no target.
        [[nodiscard]] Tile tile(std::size_t cell) const;
        [[nodiscard]] Animation animation(const std::string& name) const;
        [[nodiscard]] std::optional<Animation> animation_for(std::size_t target) const;
        // Resolve only the original target's clip; frame cells never recurse. Negative
        // time or invalid timelines throw invalid_args; original/final grid violations
        // throw bad_request. Static targets still require nonnegative time.
        [[nodiscard]] CE::Assets::Grids::CellIndex cell_at(CE::Assets::Grids::CellIndex target, std::chrono::milliseconds elapsed) const;
        // Unknown names throw out_of_range. Preserve selector failure values, otherwise
        // apply cell_at() once. No callback is retained; submit the returned cell value.
        [[nodiscard]] CE::Assets::Tilesets::Selection::Result select_tile(
            const std::string& name,
            const CE::Assets::Tilesets::Selection::Sampler& sampler,
            const CE::Assets::Tilesets::Selection::Options& options,
            std::chrono::milliseconds elapsed
        ) const;
        // References borrow this CE::Assets::Tilesets::Asset's lifetime; unknown metadata names throw out_of_range.
        [[nodiscard]] const CE::Assets::Grids::View& view(const std::string& name) const;
        [[nodiscard]] CE::Assets::Grids::CellIndex orientation(const std::string& name) const;
        [[nodiscard]] const Rule& autotile(const std::string& name) const;
        [[nodiscard]] const Definition& definition() const { return definition_; }
    };
}
