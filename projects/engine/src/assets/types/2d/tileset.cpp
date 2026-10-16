#include <assets/types/2d/tileset.h>
#include <internals/exceptions.h>

#include <algorithm>
#include <utility>

namespace CE::Assets::Tilesets {
    Animation::Animation(
        Clip definition,
        const CE::Memory::Shared<CE::Rendering::Geometry::Buffer2D>& geometry,
        const CE::Memory::Shared<CE::Rendering::Resources::Image>& texture,
        std::shared_ptr<const CE::Rendering::Resources::Material> material
    )
    : Content2D(geometry, texture, std::move(material)),
      CE::Assets::Animation::Cursor<Animation>(
          0,
          0,
          definition.frames.size(),
          definition.loop ? CE::Assets::Animation::IndexPolicy::Wrap : CE::Assets::Animation::IndexPolicy::Clamp
      ),
      definition_(std::move(definition)) {}

    std::chrono::milliseconds Animation::frame_duration() const {
        return definition_.frames.at(index_).duration;
    }

    Asset::Asset(Data data)
    : Content2D(std::move(data.geometry), std::move(data.texture), std::move(data.material)), definition_(std::move(data.definition)) {
        // Index each animated target once so tile-map selection can substitute its clip by cell.
        for (const auto& [name, animation] : definition_.animations) {
            if (!animation_targets_.emplace(animation.target, name).second) {
                throw Exceptions::invalid_args(CE_HERE, "Multiple tile animations target the same cell");
            }
        }
    }

    Tile Asset::tile(const std::size_t cell) const {
        if (cell >= definition_.grid.cell_count()) {
            throw Exceptions::bad_request(CE_HERE, "Tileset cell is outside the grid");
        }
        return Tile(cell, geometry, texture, material);
    }

    Animation Asset::animation(const std::string& name) const {
        return Animation(definition_.animations.at(name), geometry, texture, material);
    }

    std::optional<Animation> Asset::animation_for(const std::size_t target) const {
        // Tile maps choose a base cell before animation; this index finds a
        // clip only when that original cell is an animated target.
        const auto animation_name = animation_targets_.find(target);
        if (animation_name != animation_targets_.end()) {
            return animation(animation_name->second);
        }
        return std::nullopt;
    }

    CE::Assets::Grids::CellIndex Asset::cell_at(const CE::Assets::Grids::CellIndex target, const std::chrono::milliseconds elapsed) const {
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

    CE::Assets::Tilesets::Selection::Result Asset::select_tile(
        const std::string& name,
        const CE::Assets::Tilesets::Selection::Sampler& sampler,
        const CE::Assets::Tilesets::Selection::Options& options,
        const std::chrono::milliseconds elapsed
    ) const {
        if (elapsed.count() < 0)
            throw Exceptions::invalid_args(CE_HERE, "Tile selection elapsed time must be nonnegative");
        auto result = CE::Assets::Tilesets::Selection::select(autotile(name), sampler, options);
        if (auto* cell = std::get_if<CE::Assets::Grids::CellIndex>(&result))
            *cell = cell_at(*cell, elapsed);
        return result;
    }

    const CE::Assets::Grids::View& Asset::view(const std::string& name) const {
        return definition_.views.at(name);
    }

    CE::Assets::Grids::CellIndex Asset::orientation(const std::string& name) const {
        return definition_.orientations.at(name);
    }

    const Rule& Asset::autotile(const std::string& name) const {
        return definition_.autotiles.at(name);
    }
}
