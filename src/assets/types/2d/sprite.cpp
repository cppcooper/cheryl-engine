#include <assets/types/2d/sprite.h>
#include <internals/exceptions.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace CE::Assets {
    SpriteAnimation::SpriteAnimation(std::shared_ptr<const SpriteAnimationDefinition> definition)
    :
    definition_(std::move(definition)) {
        if (!definition_ || definition_->frames.empty()) {
            throw Exceptions::bad_request(CE_HERE, "A sprite animation needs at least one frame");
        }
        for (const auto& frame : definition_->frames) {
            if (frame.duration.count() <= 0) {
                throw Exceptions::bad_request(CE_HERE, "A sprite animation frame needs a positive duration");
            }
            cycle_duration_ += frame.duration;
        }
    }

    SpriteAnimation& SpriteAnimation::operator[](const std::size_t frame) {
        set_frame(frame);
        return *this;
    }

    void SpriteAnimation::set_frame(const std::size_t frame) {
        index_ = loops() ? frame % definition_->frames.size() : std::min(frame, definition_->frames.size() - 1);
        elapsed_ = {};
    }

    void SpriteAnimation::advance(const std::chrono::duration<double> delta) {
        if (!std::isfinite(delta.count()) || delta.count() < 0) {
            throw Exceptions::invalid_args(CE_HERE, "Sprite animation time must be finite and nonnegative");
        }
        const auto& frames = definition_->frames;
        if (!loops() && index_ == frames.size() - 1) {
            return;
        }
        elapsed_ += delta;
        // A long simulation tick must not loop once per skipped animation cycle.
        if (loops() && elapsed_ >= cycle_duration_) {
            elapsed_ = std::chrono::duration<double>(std::fmod(elapsed_.count(), cycle_duration_.count()));
        }
        while (elapsed_ >= frames[index_].duration) {
            elapsed_ -= frames[index_].duration;
            if (index_ + 1 == frames.size()) {
                if (!loops()) {
                    elapsed_ = {};
                    break;
                }
                index_ = 0;
            }
            else {
                ++index_;
            }
        }
    }

    CellIndex SpriteAnimation::cell() const { return definition_->frames[index_].cell; }

    std::chrono::milliseconds SpriteAnimation::frame_duration() const { return definition_->frames[index_].duration; }

    Sprite::Sprite(SpriteData data)
    :
    Asset2D(std::move(data.geometry), std::move(data.texture)),
    definition_(std::make_shared<const SpriteDefinition>(std::move(data.definition))) {
        const auto cell_count = definition_->grid.cell_count();
        if (cell_count == 0) {
            throw Exceptions::bad_request(CE_HERE, "A sprite needs at least one grid cell");
        }
        // Index the original definitions. Playback handles alias this shared allocation;
        // no clip vectors or GPU handles are copied for each animated entity.
        for (std::size_t index = 0; index < definition_->animations.size(); ++index) {
            const auto& animation = definition_->animations[index];
            if (animation.frames.empty()) {
                throw Exceptions::bad_request(CE_HERE, "A sprite animation needs at least one frame");
            }
            for (const auto& frame : animation.frames) {
                if (frame.duration.count() <= 0 || frame.cell >= cell_count) {
                    throw Exceptions::bad_request(CE_HERE, "A sprite animation has an invalid frame");
                }
            }
            const auto key = animation_key(animation.name, animation.facing);
            if (!animation_indices_.emplace(key, index).second) {
                throw Exceptions::invalid_args(CE_HERE, "Duplicate sprite animation '" + animation.name + "'");
            }
        }
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
        return std::ranges::count_if(definition_->animations, [&animation](const auto& candidate) {
            return candidate.name == animation;
        }) == 1;
    }

    SpriteAnimation Sprite::animation(
        const std::string& animation_name,
        const std::optional<std::string> facing
    ) const {
        const auto exact = animation_indices_.find(animation_key(animation_name, facing));
        if (exact != animation_indices_.end()) {
            return SpriteAnimation(std::shared_ptr<const SpriteAnimationDefinition>(
                definition_, &definition_->animations[exact->second]));
        }
        if (!facing) {
            const SpriteAnimationDefinition* match = nullptr;
            for (const auto& animation : definition_->animations) {
                if (animation.name != animation_name) {
                    continue;
                }
                if (match) {
                    throw Exceptions::bad_request(
                        CE_HERE, "Sprite animation '" + animation_name + "' requires an explicit facing");
                }
                match = &animation;
            }
            if (match) {
                return SpriteAnimation(std::shared_ptr<const SpriteAnimationDefinition>(definition_, match));
            }
        }
        throw Exceptions::bad_request(
            CE_HERE, "Sprite animation '" + animation_name + "' was not loaded for the requested facing");
    }

    SpriteAnimation Sprite::operator[](const std::string& animation_name) const {
        return animation(animation_name);
    }

    const ViewDefinition& Sprite::view(const std::string& name) const {
        return definition_->views.at(name);
    }

    CellIndex Sprite::orientation(const std::string& name) const {
        return definition_->orientations.at(name);
    }
}
