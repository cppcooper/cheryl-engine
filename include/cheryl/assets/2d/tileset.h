#pragma once
#ifndef TILESET_H
#define TILESET_H

#include <assets/abstracts.h>

#include <filesystem>
#include <tuple>
#include <memory>

namespace CE::Assets {
    template <typename T>
    using shptr = std::shared_ptr<T>;
    using TilesetData = std::tuple<shptr<Vertex2D>, uint32_t, shptr<Texture>>;

    struct Tile final : Draw2D, protected Frame {
        explicit Tile(uint16_t o, uint16_t i, uint16_t l,
            const GLuint id, const shptr<Texture> &texture)
            : Draw2D(id, texture), Frame(o,i,l) {}
        void draw(const DrawInfo &info) override;
        Tile& operator[](std::size_t frame) {
            return Frame::operator[]<Tile>(frame);
        }
    };

    struct Tileset final : Asset2D, protected Frame {
        explicit Tileset(const TilesetData& data) :
        Asset2D(std::get<0>(data),
                std::get<1>(data),
                std::get<2>(data)),
        Frame(0,0,VAONumbers::calculate_num_frames(std::get<1>(data))) {
        }
        ~Tileset() override = default;
        void draw(const DrawInfo& info) override;
        Tileset& operator[](std::size_t frame) {
            return Frame::operator[]<Tileset>(frame);
        }
        static TilesetData load_tilset(const std::filesystem::path& index_file);
    };
}
#endif
