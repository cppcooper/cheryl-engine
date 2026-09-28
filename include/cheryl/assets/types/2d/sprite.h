#pragma once

#include <assets/types/2d/base/asset2d.h>
#include <assets/manifest.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace CE::Assets {
    template <typename T>
    using shptr = std::shared_ptr<T>;

    struct SpriteData {
        shptr<Geometry2D> geometry;
        shptr<Image> texture;
        SpriteDefinition definition;
    };

    /** Per-instance playback state for one shared clip definition. Advance it on the
     * simulation thread, then publish cell() rather than sharing this mutable cursor.
     */
    class SpriteAnimation final {
    public:
        SpriteAnimation& operator[](std::size_t frame);
        void set_frame(std::size_t frame);
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

    /** Shared grid resources and clip definitions. Each animation() result has its own
     * playback cursor and retains the definition without copying its frames.
     */
    struct Sprite final : Asset2D {
        explicit Sprite(SpriteData data);

        SpriteAnimation operator[](const std::string& animation) const;
        SpriteAnimation animation(const std::string& animation, std::optional<std::string> facing = std::nullopt) const;
        [[nodiscard]] bool has_animation(const std::string& animation,
                                         std::optional<std::string> facing = std::nullopt) const;
        [[nodiscard]] const ViewDefinition& view(const std::string& name) const;
        [[nodiscard]] CellIndex orientation(const std::string& name) const;
        [[nodiscard]] const SpriteDefinition& definition() const { return *definition_; }

    private:
        [[nodiscard]] static std::string animation_key(const std::string& animation,
                                                       const std::optional<std::string>& facing);

        std::shared_ptr<const SpriteDefinition> definition_;
        std::unordered_map<std::string, std::size_t> animation_indices_;
    };
}
