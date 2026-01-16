#include <assets/2d/tileset.h>
#include <math/anchor.h>
#include <fstream>
#include <format>
#include <core/resources/asset-management/texture-mgr.h>
#include <core/resources/memory.h>
#include <nlohmann/json.hpp>

namespace CE::Assets {
    void Tile::draw(const DrawInfo& info) {
        glBindVertexArray(id_vao);
        texture->bind();
        info.use_shader();
        glDrawArrays(GL_QUADS,
            VAONumbers::calculate_num_vertices(offset_ + index_),
            VAONumbers::vertices_per_quad);
    }

    void Tileset::draw(const DrawInfo& info) {
        Tile(offset_, index_, limit_, vao.id, texture).draw(info);
    }

    TilesetData Tileset::load_tilset(const std::filesystem::path& index_file) {
        std::ifstream file(index_file);
        using json = nlohmann::json;
        json data = json::parse(file);
        if (data["meta"].size() >= 4) {
            shptr<Texture> texture = TextureMgr::get().get_asset(data["meta"]["texture"]);
            std::size_t rows = data["tileset"]["r"];
            std::size_t columns = data["tileset"]["c"];
            std::size_t width = data["tileset"]["w"];
            std::size_t height = data["tileset"]["h"];
            uint32_t total_frames = rows * columns;
            math::AnchorType anchor = math::get_anchor(data["meta"]["anchor"]);

            std::size_t vertices_bytes = sizeof(Quad) * total_frames;
            auto b = Mem::ExactMMgr::get().checkout_chunk(vertices_bytes, alignof(float));
            auto vertices = std::shared_ptr<Vertex2D>(static_cast<Vertex2D*>(b.head.get()),[b](void*) {
                Mem::ExactMMgr::get().return_chunk(b);
            });

            std::size_t frame_counter = 0;
            for(int r = 0; r < rows; ++r) {
                for(int c = 0; c < columns; c++) {
                    int x0 = width * c, y0 = height * r;
                    math::Anchor::MakeAnchor(anchor,
                                             reinterpret_cast<float*>(vertices.get() + (frame_counter++ * VAONumbers::vertices_per_quad)),
                                             texture->width, texture->height, width,height, x0, y0);
                }
            }
            return {vertices, total_frames*VAONumbers::vertices_per_quad, texture};
        }
        return {};
    }
}
