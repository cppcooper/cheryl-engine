#include <assets/2d/tileset.h>
#include <assets/primitives/vertex.h>
#include <internals/exceptions.h>

#include <algorithm>
#include <utility>

namespace CE::Assets {
    void Tile::draw(const DrawInfo& info) {
        geometry->bind(*texture);
        info.use_shader();
        geometry->draw(VAONumbers::calculate_num_vertices(offset_), VAONumbers::vertices_per_quad);
    }

    TileAnimation::TileAnimation(TileAnimationDefinition definition, const shptr<Geometry2D>& geometry,
                                 const shptr<Image>& texture) :
        Draw2D(geometry, texture), Frame(0, 0, definition.frames.size()), definition_(std::move(definition)) {
        if (definition_.frames.empty()) {
            throw Exceptions::invalid_args(CE_HERE, "A tile animation must contain at least one frame");
        }
    }

    void TileAnimation::draw(const DrawInfo& info) {
        Tile(definition_.frames[index_].cell, geometry, texture).draw(info);
    }

    Tile TileAnimation::operator[](const std::size_t frame) {
        // Looping clips wrap the requested index; a finite clip holds its
        // final frame after the sequence has been exhausted.
        index_ = definition_.loop ? frame % definition_.frames.size() : std::min(frame, definition_.frames.size() - 1);
        return Tile(definition_.frames[index_].cell, geometry, texture);
    }

    std::chrono::milliseconds TileAnimation::frame_duration() const {
        return definition_.frames.at(index_).duration;
    }

    Tileset::Tileset(TilesetData data) :
        Asset2D(std::move(data.geometry), std::move(data.texture)),
        Frame(0, 0, data.definition.grid.cell_count()),
        definition_(std::move(data.definition)) {
        // Index each animated target once so tile-map selection can substitute its clip by cell.
        for (const auto& [name, animation] : definition_.animations) {
            if (!animation_targets_.emplace(animation.target, name).second) {
                throw Exceptions::invalid_args(CE_HERE, "Multiple tile animations target the same cell");
            }
        }
    }

    void Tileset::draw(const DrawInfo& info) {
        Tile(index_, geometry, texture).draw(info);
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
