#include <assets/types/2d/tileset.h>
#include <internals/exceptions.h>

#include <algorithm>
#include <utility>

namespace CE::Assets {
    TileAnimation::TileAnimation(TileAnimationDefinition definition, const shptr<Geometry2D>& geometry, const shptr<Image>& texture)
    : Asset2D(geometry, texture),
      Frame(0, 0, definition.frames.size(), definition.loop ? FrameIndexPolicy::Wrap : FrameIndexPolicy::Clamp),
      definition_(std::move(definition)) {}

    std::chrono::milliseconds TileAnimation::frame_duration() const {
        return definition_.frames.at(index_).duration;
    }

    Tileset::Tileset(TilesetData data)
    : Asset2D(std::move(data.geometry), std::move(data.texture)), definition_(std::move(data.definition)) {
        // Index each animated target once so tile-map selection can substitute its clip by cell.
        for (const auto& [name, animation] : definition_.animations) {
            if (!animation_targets_.emplace(animation.target, name).second) {
                throw Exceptions::invalid_args(CE_HERE, "Multiple tile animations target the same cell");
            }
        }
    }

    Tile Tileset::tile(const std::size_t cell) const {
        if (cell >= definition_.grid.cell_count()) {
            throw Exceptions::bad_request(CE_HERE, "Tileset cell is outside the grid");
        }
        return Tile(cell, geometry, texture);
    }

    TileAnimation Tileset::animation(const std::string& name) const {
        return TileAnimation(definition_.animations.at(name), geometry, texture);
    }

    std::optional<TileAnimation> Tileset::animation_for(const std::size_t target) const {
        // Tile maps choose a base cell before animation; this index finds a
        // clip only when that original cell is an animated target.
        const auto animation_name = animation_targets_.find(target);
        if (animation_name != animation_targets_.end()) {
            return animation(animation_name->second);
        }
        return std::nullopt;
    }

    CellIndex Tileset::cell_at(const CellIndex target, const std::chrono::milliseconds elapsed) const {
        if (elapsed.count() < 0)
            throw Exceptions::invalid_args(CE_HERE, "Tile selection elapsed time must be nonnegative");
        const auto count = definition_.grid.cell_count();
        if (target >= count)
            throw Exceptions::bad_request(CE_HERE, "Selected tile target is outside the grid");

        const auto animation_name = animation_targets_.find(target);
        if (animation_name == animation_targets_.end())
            return target;
        const auto cell = definition_.animations.at(animation_name->second).cell_at(elapsed);
        if (cell >= count)
            throw Exceptions::bad_request(CE_HERE, "Selected tile animation frame is outside the grid");
        return cell;
    }

    TileSelectionResult Tileset::select_tile(
        const std::string& name,
        const TerrainSampler& sampler,
        const TileSelectionOptions& options,
        const std::chrono::milliseconds elapsed
    ) const {
        if (elapsed.count() < 0)
            throw Exceptions::invalid_args(CE_HERE, "Tile selection elapsed time must be nonnegative");
        auto result = CE::Assets::select_tile(autotile(name), sampler, options);
        if (auto* cell = std::get_if<CellIndex>(&result))
            *cell = cell_at(*cell, elapsed);
        return result;
    }

    const ViewDefinition& Tileset::view(const std::string& name) const {
        return definition_.views.at(name);
    }

    CellIndex Tileset::orientation(const std::string& name) const {
        return definition_.orientations.at(name);
    }

    const AutotileDefinition& Tileset::autotile(const std::string& name) const {
        return definition_.autotiles.at(name);
    }
}
