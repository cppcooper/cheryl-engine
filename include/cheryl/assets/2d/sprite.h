#pragma once

#include <assets/abstracts.h>
#include <assets/manifest.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace CE::Assets {
    template <typename T>
    using shptr = std::shared_ptr<T>;

    struct SpriteData {
        shptr<Geometry2D> geometry;
        shptr<Image> texture;
        SpriteDefinition definition;
    };

    struct SpriteFrame final : Draw2D, Frame<SpriteFrame> {
        explicit SpriteFrame(std::size_t cell, const shptr<Geometry2D>& geometry, const shptr<Image>& texture) :
            Draw2D(geometry, texture), Frame(cell, 0, 1) {}

        void draw(const DrawInfo& info) override;
        [[nodiscard]] std::size_t cell() const { return offset_; }
    };

    /** A selected clip with mutable frame index; the caller chooses when to advance it.
     * Indexing selects a cell, with looping or final-frame clamping from the definition.
     */
    struct SpriteAnimation final : Draw2D, Frame<SpriteAnimation> {
        explicit SpriteAnimation(SpriteAnimationDefinition definition, const shptr<Geometry2D>& geometry,
                                 const shptr<Image>& texture);

        void draw(const DrawInfo& info) override;
        [[nodiscard]] const SpriteAnimationDefinition& definition() const { return definition_; }
        [[nodiscard]] std::chrono::milliseconds frame_duration() const;
        [[nodiscard]] bool loops() const { return definition_.loop; }

    private:
        SpriteAnimationDefinition definition_;
    };

    /** Cached geometry/image and named animation definitions for one sprite grid.
     * Animation lookup returns a value; keep that value if frame selection should persist.
     */
    struct Sprite final : Asset2D, Frame<Sprite> {
        using Frame::operator[];
        explicit Sprite(SpriteData data);

        void draw(const DrawInfo& info) override;
        SpriteAnimation operator[](const std::string& animation) const;
        SpriteAnimation animation(const std::string& animation, std::optional<std::string> facing = std::nullopt) const;
        [[nodiscard]] bool has_animation(const std::string& animation,
                                         std::optional<std::string> facing = std::nullopt) const;
        [[nodiscard]] const ViewDefinition& view(const std::string& name) const;
        [[nodiscard]] CellIndex orientation(const std::string& name) const;
        [[nodiscard]] const SpriteDefinition& definition() const { return definition_; }

    private:
        [[nodiscard]] static std::string animation_key(const std::string& animation,
                                                       const std::optional<std::string>& facing);

        SpriteDefinition definition_;
        std::vector<SpriteAnimation> animations_;
        std::unordered_map<std::string, std::size_t> animation_indices_;
    };
}
