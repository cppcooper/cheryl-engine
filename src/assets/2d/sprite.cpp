#include <assets/2d/sprite.h>
#include <assets/primitives/vertex.h>
#include <internals/exceptions.h>

#include <algorithm>
#include <utility>

namespace CE::Assets {
    void SpriteFrame::draw(const DrawInfo& info) {
        geometry->bind(*texture);
        info.use_shader();
        geometry->draw(VAONumbers::calculate_num_vertices(offset_), VAONumbers::vertices_per_quad);
    }

    SpriteAnimation::SpriteAnimation(SpriteAnimationDefinition definition, const shptr<Geometry2D>& geometry,
                                     const shptr<Image>& texture) :
        Draw2D(geometry, texture),
        Frame(0, 0, definition.frames.size(), definition.loop ? FrameIndexPolicy::Wrap : FrameIndexPolicy::Clamp),
        definition_(std::move(definition)) {}

    void SpriteAnimation::draw(const DrawInfo& info) {
        SpriteFrame(definition_.frames[index_].cell, geometry, texture).draw(info);
    }

    std::chrono::milliseconds SpriteAnimation::frame_duration() const {
        return definition_.frames.at(index_).duration;
    }

    Sprite::Sprite(SpriteData data) :
        Asset2D(std::move(data.geometry), std::move(data.texture)),
        Frame(0, 0, data.definition.grid.cell_count(), data.definition.),
        definition_(std::move(data.definition)) {
        // Map clip name and facing to a single lookup index, retaining shared GPU resources
        // in each clip value while the original manifest definition remains inspectable.
        animations_.reserve(definition_.animations.size());
        for (const auto& animation_definition : definition_.animations) {
            const auto key = animation_key(animation_definition.name, animation_definition.facing);
            if (animation_indices_.contains(key)) {
                throw Exceptions::invalid_args(CE_HERE,
                                               "Duplicate sprite animation '" + animation_definition.name + "'");
            }
            animation_indices_.emplace(key, animations_.size());
            animations_.emplace_back(animation_definition, geometry, texture);
        }
    }

    void Sprite::draw(const DrawInfo& info) {
        SpriteFrame(index_, geometry, texture).draw(info);
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
        // A faceless query may still name one facing-specific clip; multiple
        // matches are ambiguous and should be requested with a facing.
        return std::ranges::count_if(animations_, [&animation](const auto& candidate) {
            return candidate.definition().name == animation;
        }) == 1;
    }

    SpriteAnimation Sprite::animation(const std::string& animation_name,
                                      const std::optional<std::string> facing) const {
        // Prefer an exact (name, facing) match. With no facing, accept a clip by name only
        // when there is exactly one candidate; otherwise require the caller to disambiguate.
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
                    throw Exceptions::bad_request(
                        CE_HERE, "Sprite animation '" + animation_name + "' requires an explicit facing");
                }
                match = &animation;
            }
            if (match) {
                return *match;
            }
        }
        throw Exceptions::bad_request(
            CE_HERE, "Sprite animation '" + animation_name + "' was not loaded for the requested facing");
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
