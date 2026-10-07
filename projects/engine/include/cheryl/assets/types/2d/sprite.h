#pragma once

#include <assets/types/2d/base/asset2d.h>
#include <assets/definitions/sprite.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace CE::Assets {
    template <typename T> using shptr = std::shared_ptr<T>;

    struct SpriteData {
        shptr<Geometry2D> geometry;
        shptr<Image> texture;
        SpriteDefinition definition;
    };

    /** Independent cursor retaining shared immutable clip metadata, even after its
     * Sprite is released. Copies copy playback state without retaining GPU handles.
     * Advance on its simulation owner, then publish cell(); mutation is unsynchronized.
     */
    class SpriteAnimation final {
    public:
        SpriteAnimation& operator[](std::size_t frame);
        // Wrap looping clips, clamp nonlooping clips, and reset time within the frame.
        void set_frame(std::size_t frame);
        // Finite nonnegative seconds; invalid time throws without changing the cursor.
        // Exact duration boundaries advance; nonlooping clips stop at their last frame.
        void advance(std::chrono::duration<double> elapsed);
        [[nodiscard]] std::size_t index() const { return index_; }
        [[nodiscard]] CellIndex cell() const;
        [[nodiscard]] const SpriteAnimationDefinition& definition() const { return *definition_; }
        [[nodiscard]] std::chrono::milliseconds frame_duration() const;
        [[nodiscard]] bool loops() const { return definition_->loop; }

    private:
        friend struct Sprite;
        explicit SpriteAnimation(std::shared_ptr<const SpriteAnimationDefinition> definition);

        std::shared_ptr<const SpriteAnimationDefinition> definition_;
        std::size_t index_ = 0;
        std::chrono::duration<double> elapsed_{};
        std::chrono::duration<double> cycle_duration_{};
    };

    /** Retains a definition snapshot and grid resources. Construction checks nonempty
     * cells/clips, frame cells/durations and duplicate clip keys, not all manifest
     * fields or backend compatibility. Read concurrently only while inputs are stable.
     */
    struct Sprite final : Asset2D {
        explicit Sprite(SpriteData data);

        SpriteAnimation operator[](const std::string& animation) const;
        // Prefer an exact name/facing. Without a facing, one matching clip is allowed;
        // missing/ambiguous requests throw. operator[] uses this faceless lookup.
        SpriteAnimation animation(const std::string& animation, std::optional<std::string> facing = std::nullopt) const;
        [[nodiscard]] bool has_animation(const std::string& animation, std::optional<std::string> facing = std::nullopt) const;
        // Metadata references borrow this Sprite's lifetime; missing names throw out_of_range.
        [[nodiscard]] const ViewDefinition& view(const std::string& name) const;
        [[nodiscard]] CellIndex orientation(const std::string& name) const;
        [[nodiscard]] const SpriteDefinition& definition() const { return *definition_; }

    private:
        [[nodiscard]] static std::string animation_key(const std::string& animation, const std::optional<std::string>& facing);

        std::shared_ptr<const SpriteDefinition> definition_;
        std::unordered_map<std::string, std::size_t> animation_indices_;
    };
}
