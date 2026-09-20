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
        shptr<Vertex2D> vertices;
        std::uint32_t vertex_count{};
        shptr<Texture> texture;
        SpriteDefinition definition;
    };

    struct SpriteFrame final : Draw2D, protected Frame {
        explicit SpriteFrame(std::size_t cell, GLuint id, const shptr<Texture>& texture)
            : Draw2D(id, texture), Frame(cell, 0, 1) {}

        void draw(const DrawInfo& info) override;
        SpriteFrame& operator[](std::size_t frame);
        [[nodiscard]] std::size_t cell() const { return offset_; }
    };

    struct SpriteAnimation final : Draw2D, protected Frame {
        explicit SpriteAnimation(SpriteAnimationDefinition definition, GLuint id,
                                 const shptr<Texture>& texture);

        void draw(const DrawInfo& info) override;
        SpriteFrame operator[](std::size_t frame);
        [[nodiscard]] const SpriteAnimationDefinition& definition() const { return definition_; }
        [[nodiscard]] std::chrono::milliseconds frame_duration() const;
        [[nodiscard]] bool loops() const { return definition_.loop; }

    private:
        SpriteAnimationDefinition definition_;
    };

    struct Sprite final : Asset2D, protected Frame {
        explicit Sprite(SpriteData data);

        void draw(const DrawInfo& info) override;
        SpriteFrame operator[](std::size_t frame);
        SpriteAnimation operator[](const std::string& animation) const;
        SpriteAnimation animation(const std::string& animation,
                                  std::optional<std::string> facing = std::nullopt) const;
        [[nodiscard]] bool has_animation(
            const std::string& animation,
            std::optional<std::string> facing = std::nullopt) const;
        [[nodiscard]] const ViewDefinition& view(const std::string& name) const;
        [[nodiscard]] CellIndex orientation(const std::string& name) const;
        [[nodiscard]] const SpriteDefinition& definition() const { return definition_; }

    private:
        [[nodiscard]] static std::string animation_key(
            const std::string& animation, const std::optional<std::string>& facing);

        SpriteDefinition definition_;
        std::vector<SpriteAnimation> animations_;
        std::unordered_map<std::string, std::size_t> animation_indices_;
    };
}
