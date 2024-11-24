#pragma once
#ifndef SPRITE_H
#define SPRITE_H

#include <assets/abstracts.h>
#include <core/resources/allocators.h>

#include <filesystem>
#include <tuple>
#include <memory>

namespace CE::Assets {
    template <typename T>
    using shptr = std::shared_ptr<T>;

    struct SpriteFrame final : Draw2D, protected Frame {
        explicit SpriteFrame(uint16_t o, uint16_t i, uint16_t l,
            const GLuint id, const shptr<Texture> &texture)
            : Draw2D(id, texture), Frame(o,i,l) {}
        void draw(const DrawInfo &info) override;
        SpriteFrame& operator[](std::size_t frame);
    };

    struct SpriteAnimation final : Draw2D, protected Frame {
        explicit SpriteAnimation(const std::size_t o, const std::size_t l,
            const GLuint id, const shptr<Texture> &texture)
            : Draw2D(id, texture), Frame(o,0,l) {}
        void draw(const DrawInfo &info) override;
        SpriteFrame operator[](std::size_t frame);
    };

    using SpriteData = std::tuple<shptr<Vertex2D>,uint32_t,shptr<Texture>,std::vector<std::tuple<std::string,uint16_t,uint16_t>>>;
    struct Sprite final : Asset2D, protected Frame {
        explicit Sprite(const SpriteData &data) :
        Asset2D(
            std::get<0>(data),
            std::get<1>(data),
            std::get<2>(data)),
        Frame(0,0, std::get<1>(data) / VAONumbers::vertices_per_quad), animations() {
            auto &anim = std::get<3>(data);
            animations.reserve(anim.size());
            for(auto &[animation,frames,offset] : anim) {
                animations_map[animation] = animations.size(); // map the name to the vector index
                animations.emplace_back(offset, frames, vao.id, texture);
            }
        }
        void draw(const DrawInfo &info) override;
        SpriteFrame operator[](std::size_t frame);
        SpriteAnimation operator[](const std::string& animation);
        static SpriteData load_sprite(const std::filesystem::path &file);
    private:
        std::vector<SpriteAnimation, Mem::ObjectPoolAllocator<SpriteAnimation>> animations;
        std::unordered_map<std::string, std::size_t> animations_map;
    };
}
#endif
