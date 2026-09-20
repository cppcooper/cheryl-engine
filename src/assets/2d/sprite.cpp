#include <assets/2d/sprite.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace CE::Assets {
    void SpriteFrame::draw(const DrawInfo& info) {
        glBindVertexArray(id_vao);
        texture->bind();
        info.use_shader();
        glDrawArrays(GL_QUADS, static_cast<GLint>(VAONumbers::calculate_num_vertices(offset_)),
                     VAONumbers::vertices_per_quad);
    }

    SpriteFrame& SpriteFrame::operator[](const std::size_t frame) {
        set_frame(frame);
        return *this;
    }

    SpriteAnimation::SpriteAnimation(SpriteAnimationDefinition definition, const GLuint id,
                                     const shptr<Texture>& texture) :
        Draw2D(id, texture), Frame(0, 0, definition.frames.size()), definition_(std::move(definition)) {
        if (definition_.frames.empty()) {
            throw std::invalid_argument("A sprite animation must contain at least one frame");
        }
    }

    void SpriteAnimation::draw(const DrawInfo& info) {
        SpriteFrame(definition_.frames[index_].cell, id_vao, texture).draw(info);
    }

    SpriteFrame SpriteAnimation::operator[](const std::size_t frame) {
        index_ = definition_.loop ? frame % definition_.frames.size() : std::min(frame, definition_.frames.size() - 1);
        return SpriteFrame(definition_.frames[index_].cell, id_vao, texture);
    }

    std::chrono::milliseconds SpriteAnimation::frame_duration() const {
        return definition_.frames.at(index_).duration;
    }

    Sprite::Sprite(SpriteData data) :
        Asset2D(data.vertices, data.vertex_count, data.texture), Frame(0, 0, data.definition.grid.cell_count()),
        definition_(std::move(data.definition)) {
        animations_.reserve(definition_.animations.size());
        for (const auto& animation_definition : definition_.animations) {
            const auto key = animation_key(animation_definition.name, animation_definition.facing);
            if (animation_indices_.contains(key)) {
                throw std::invalid_argument("Duplicate sprite animation '" + animation_definition.name + "'");
            }
            animation_indices_.emplace(key, animations_.size());
            animations_.emplace_back(animation_definition, vao.id, texture);
        }
    }

    void Sprite::draw(const DrawInfo& info) {
        SpriteFrame(index_, vao.id, texture).draw(info);
    }

    SpriteFrame Sprite::operator[](const std::size_t frame) {
        set_frame(frame);
        return SpriteFrame(index_, vao.id, texture);
    }

    std::string Sprite::animation_key(const std::string& animation, const std::optional<std::string>& facing) {
        return animation + '\x1f' + facing.value_or("");
    }

    bool Sprite::has_animation(const std::string& animation, const std::optional<std::string> facing) const {
        if (animation_indices_.contains(animation_key(animation, facing))) {
            return true;
        }
        if (facing) {
            return false;
        }
        return std::ranges::count_if(animations_, [&animation](const auto& candidate) {
                   return candidate.definition().name == animation;
               }) == 1;
    }

    SpriteAnimation Sprite::animation(const std::string& animation_name,
                                      const std::optional<std::string> facing) const {
        const auto exact = animation_indices_.find(animation_key(animation_name, facing));
        if (exact != animation_indices_.end()) {
            return animations_.at(exact->second);
        }
        if (!facing) {
            const SpriteAnimation* match = nullptr;
            for (const auto& animation : animations_) {
                if (animation.definition().name != animation_name) {
                    continue;
                }
                if (match) {
                    throw std::out_of_range("Sprite animation '" + animation_name + "' requires an explicit facing");
                }
                match = &animation;
            }
            if (match) {
                return *match;
            }
        }
        throw std::out_of_range("Sprite animation '" + animation_name + "' was not loaded for the requested facing");
    }

    SpriteAnimation Sprite::operator[](const std::string& animation_name) const {
        return animation(animation_name);
    }

    const ViewDefinition& Sprite::view(const std::string& name) const {
        return definition_.views.at(name);
    }

    CellIndex Sprite::orientation(const std::string& name) const {
        return definition_.orientations.at(name);
    }
}
