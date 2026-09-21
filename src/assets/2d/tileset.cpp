#include <assets/2d/tileset.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace CE::Assets {
    void Tile::draw(const DrawInfo& info) {
        glBindVertexArray(id_vao);
        texture->bind();
        info.use_shader();
        glDrawArrays(GL_TRIANGLES, static_cast<GLint>(VAONumbers::calculate_num_vertices(offset_)),
                     VAONumbers::vertices_per_quad);
    }

    TileAnimation::TileAnimation(TileAnimationDefinition definition, const GLuint id, const shptr<Texture>& texture) :
        Draw2D(id, texture), Frame(0, 0, definition.frames.size()), definition_(std::move(definition)) {
        if (definition_.frames.empty()) {
            throw std::invalid_argument("A tile animation must contain at least one frame");
        }
    }

    void TileAnimation::draw(const DrawInfo& info) {
        Tile(definition_.frames[index_].cell, id_vao, texture).draw(info);
    }

    Tile TileAnimation::operator[](const std::size_t frame) {
        index_ = definition_.loop ? frame % definition_.frames.size() : std::min(frame, definition_.frames.size() - 1);
        return Tile(definition_.frames[index_].cell, id_vao, texture);
    }

    std::chrono::milliseconds TileAnimation::frame_duration() const {
        return definition_.frames.at(index_).duration;
    }

    Tileset::Tileset(TilesetData data) :
        Asset2D(data.vertices, data.vertex_count, data.texture), Frame(0, 0, data.definition.grid.cell_count()),
        definition_(std::move(data.definition)) {
        for (const auto& [name, animation] : definition_.animations) {
            if (!animation_targets_.emplace(animation.target, name).second) {
                throw std::invalid_argument("Multiple tile animations target the same cell");
            }
        }
    }

    void Tileset::draw(const DrawInfo& info) {
        Tile(index_, vao.id, texture).draw(info);
    }

    Tile Tileset::tile(const std::size_t cell) const {
        if (cell >= definition_.grid.cell_count()) {
            throw std::out_of_range("Tileset cell is outside the grid");
        }
        return Tile(cell, vao.id, texture);
    }

    TileAnimation Tileset::animation(const std::string& name) const {
        return TileAnimation(definition_.animations.at(name), vao.id, texture);
    }

    std::optional<TileAnimation> Tileset::animation_for(const std::size_t target) const {
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
