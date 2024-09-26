#include <assets/2d/sprite.h>
#include <resources/assets.h>
#include <resources/allocators.h>
#include <resources/assets/texture-mgr.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <math/anchor.h>
#include <internals.h>


namespace CE::Assets {
    void SpriteFrame::draw(const DrawInfo &info) {
        glBindVertexArray(id_vao);
        texture->bind();
        info.use_shader();
        glDrawArrays(GL_QUADS,
            VAONumbers::calculate_num_vertices(offset_ + index_),
            VAONumbers::vertices_per_quad);
    }

    SpriteFrame& SpriteFrame::operator[](std::size_t frame) {
        set_frame(frame);
        return *this;
    }

    void SpriteAnimation::draw(const DrawInfo &info) {
        SpriteFrame(offset_, index_, limit_, id_vao, texture).draw(info);
    }

    SpriteFrame SpriteAnimation::operator[](std::size_t frame) {
        set_frame(frame);
        return SpriteFrame(offset_, frame, limit_, id_vao, texture);
    }

    void Sprite::draw(const DrawInfo &info) {
        operator[](index_).draw(info);
    }

    SpriteFrame Sprite::operator[](std::size_t frame) {
        set_frame(frame);
        return SpriteFrame(index_,0,1,vao.id,texture);
    }

    SpriteAnimation Sprite::operator[](const std::string& animation) {
        auto &anim = animations[animations_map[animation]];
        return anim;
    }

    SpriteData Sprite::load_sprite(const std::filesystem::path& index_file) {
        std::ifstream file(index_file);
        if (!file.is_open()) {
            throw Exceptions::runtime_exception(CE_HERE, std::format(R"(Unable to load "{}")", index_file.string()).c_str());
        }
        using json = nlohmann::json;
        if (json data = json::parse(file); data["meta"].size() >= 4) {
            shptr<Texture> texture = TextureMgr::get().get_asset(data["meta"]["texture"]);
            std::size_t total_frames = data["meta"]["frames"];
            AnchorType anchor = get_anchor(data["meta"]["anchor"]);

            std::size_t vertices_bytes = sizeof(Quad) * total_frames;
            auto b = Mem::ExactMMgr::get().checkout_chunk(vertices_bytes, alignof(float));
            auto vertices = std::shared_ptr<Vertex2D>(static_cast<Vertex2D*>(b.head.get()),[b](void*) {
                Mem::ExactMMgr::get().return_chunk(b);
            });

            std::vector<std::tuple<std::string,uint16_t,uint16_t>> animations;
            uint32_t frame_offset = 0;
            for(auto &[key,value] : data["animations"].items()) {
                uint16_t frames = value["frames"];
                animations.emplace_back(key, frames, frame_offset);
                uint16_t x = value["frame0"]["x"];
                uint16_t y = value["frame0"]["y"];
                uint16_t w = value["w"];
                uint16_t h = value["h"];
                for(int i = 0; i < frames; ++i) {
                    std::size_t offset = frame_offset + i;
                    if (offset >= total_frames) [[unlikely]] {
                        throw Exceptions::bad_request(CE_HERE, "Out of space. This can only mean one thing: the vertices array was too small.");
                    }
                    Anchor::MakeAnchor(anchor,
                        reinterpret_cast<float*>(vertices.get() + (offset * VAONumbers::vertices_per_quad)),
                        texture->width, texture->height, w,h, x+(i*w), y);
                }
                frame_offset += frames;
            }
            const uint32_t num_verts = frame_offset * VAONumbers::vertices_per_quad;
            return {vertices, num_verts, texture, animations};
        }
        throw Exceptions::failed_operation(CE_HERE, "File is definitely missing data.");
    }
}
