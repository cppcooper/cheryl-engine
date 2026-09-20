#pragma once

#include <assets/abstracts.h>
#include <assets/manifest.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace CE::Assets {
    template <typename T>
    using shptr = std::shared_ptr<T>;

    struct TilesetData {
        shptr<Vertex2D> vertices;
        std::uint32_t vertex_count{};
        shptr<Texture> texture;
        TilesetDefinition definition;
    };

    struct Tile final : Draw2D, protected Frame {
        explicit Tile(std::size_t cell, GLuint id, const shptr<Texture>& texture) :
            Draw2D(id, texture), Frame(cell, 0, 1) {}

        void draw(const DrawInfo& info) override;
        Tile& operator[](std::size_t frame) { return Frame::operator[]<Tile>(frame); }
        [[nodiscard]] std::size_t cell() const { return offset_; }
    };

    struct TileAnimation final : Draw2D, protected Frame {
        explicit TileAnimation(TileAnimationDefinition definition, GLuint id, const shptr<Texture>& texture);

        void draw(const DrawInfo& info) override;
        Tile operator[](std::size_t frame);
        [[nodiscard]] const TileAnimationDefinition& definition() const { return definition_; }
        [[nodiscard]] std::chrono::milliseconds frame_duration() const;
        [[nodiscard]] bool loops() const { return definition_.loop; }

    private:
        TileAnimationDefinition definition_;
    };

    struct Tileset final : Asset2D, protected Frame {
        explicit Tileset(TilesetData data);
        ~Tileset() override = default;

        void draw(const DrawInfo& info) override;
        Tileset& operator[](std::size_t frame) { return Frame::operator[]<Tileset>(frame); }
        [[nodiscard]] Tile tile(std::size_t cell) const;
        [[nodiscard]] TileAnimation animation(const std::string& name) const;
        [[nodiscard]] std::optional<TileAnimation> animation_for(std::size_t target) const;
        [[nodiscard]] const ViewDefinition& view(const std::string& name) const;
        [[nodiscard]] CellIndex orientation(const std::string& name) const;
        [[nodiscard]] const AutotileDefinition& autotile(const std::string& name) const;
        [[nodiscard]] const TilesetDefinition& definition() const { return definition_; }

    private:
        TilesetDefinition definition_;
        std::unordered_map<CellIndex, std::string> animation_targets_;
    };
}
