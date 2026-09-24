#include <assets/2d/stbfont.h>
#include <assets/abstracts/resource-provider.h>
#include <assets/primitives/vertex.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include <core/resources/memory.h>
#include <core/resources/memory/managed-block.hpp>
#include <ext/matrix_transform.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <internals/exceptions.h>
#include <utility>
#include <vector>

namespace CE::Assets {
    namespace {
        std::vector<unsigned char> read_font_file(const std::filesystem::path& path) {
            std::ifstream input(path, std::ios::binary | std::ios::ate);
            if (!input)
                throw Exceptions::runtime_exception(CE_HERE, "Unable to open font file '" + path.string() + "'");

            const auto end = input.tellg();
            if (end <= 0)
                throw Exceptions::runtime_exception(CE_HERE, "Font file is empty: '" + path.string() + "'");
            std::vector<unsigned char> bytes(static_cast<std::size_t>(end));
            input.seekg(0, std::ios::beg);
            if (!input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
                throw Exceptions::runtime_exception(CE_HERE, "Unable to read font file '" + path.string() + "'");
            }
            return bytes;
        }

        void set_glyph_vertices(Vertex2D* vertices, const stbtt_aligned_quad& quad) {
            // Flip stb's downward-positive glyph Y into the engine's upward-positive local space;
            // expand the four corners into two triangles matching the non-indexed 2D geometry.
            const float left = quad.x0;
            const float right = quad.x1;
            const float bottom = -quad.y1;
            const float top = -quad.y0;
            vertices[0] = {left, bottom, 0.0f, quad.s0, quad.t1};
            vertices[1] = {right, bottom, 0.0f, quad.s1, quad.t1};
            vertices[2] = {right, top, 0.0f, quad.s1, quad.t0};
            vertices[3] = vertices[0];
            vertices[4] = vertices[2];
            vertices[5] = {left, top, 0.0f, quad.s0, quad.t0};
        }
    }

    STBFont::STBFont(STBFontData data) :
        Font({data.geometry, data.texture}), advances_(data.advances),
        line_height_(data.line_height) {
    }

    void STBFont::print(std::string text, FontDrawInfo* format) {
        if (!format)
            throw Exceptions::invalid_args(CE_HERE, "A font draw requires formatting information");
        print_message_ = std::move(text);
        print_angle_ = format->angle;
        draw(*format);
    }

    void STBFont::draw(const DrawInfo& info) {
        if (!info.material)
            throw Exceptions::invalid_args(CE_HERE, "A font draw requires a shader program");
        info.material->use();
        info.material->set_uniform_value("in_Alpha", info.alpha);
        info.material->set_uniform_value("in_Scale", 1.0f);
        info.material->set_uniform_value("mytexture", 0);
        geometry->bind(*texture);

        // Compose caller transform, print origin, rotation, and scale once. Per-glyph transforms
        // then add pen offsets without moving the atlas geometry on the CPU.
        auto text_matrix = glm::translate(info.model_matrix, info.position);
        text_matrix = glm::rotate(text_matrix, print_angle_, glm::vec3(0.0f, 0.0f, 1.0f));
        text_matrix = glm::scale(text_matrix, glm::vec3(info.scale, info.scale, 1.0f));
        float cursor_x = 0.0f;
        float cursor_y = 0.0f;
        constexpr auto fallback_character = static_cast<unsigned char>('?');
        const auto space_index = static_cast<std::size_t>(' ' - first_font_character);

        // Move the pen for whitespace, otherwise select one baked glyph (or '?') and draw
        // its pre-uploaded six-vertex range with a per-glyph model matrix.
        for (const unsigned char requested_character : print_message_) {
            if (requested_character == '\n') {
                cursor_x = 0.0f;
                cursor_y -= line_height_;
                continue;
            }
            if (requested_character == '\r')
                continue;
            if (requested_character == '\t') {
                cursor_x += advances_[space_index] * 4.0f;
                continue;
            }

            const auto letter = requested_character < first_font_character || requested_character > last_font_character
                ? fallback_character
                : requested_character;
            const auto index = static_cast<std::size_t>(letter - first_font_character);
            if (letter != ' ') {
                const auto model_matrix = glm::translate(text_matrix, glm::vec3(cursor_x, cursor_y, 0.0f));
                info.material->set_uniform_matrix("modelMatrix", model_matrix);
                geometry->draw(index * VAONumbers::vertices_per_quad, VAONumbers::vertices_per_quad);
            }
            cursor_x += advances_[index];
        }
    }

    STBFontData STBFont::load_font(const std::filesystem::path& font_path, const int font_size,
                                  ResourceProvider& provider) {
        if (font_size <= 0)
            throw Exceptions::invalid_args(CE_HERE, "Font size must be positive");
        const auto font_bytes = read_font_file(font_path);
        const int font_offset = stbtt_GetFontOffsetForIndex(font_bytes.data(), 0);
        stbtt_fontinfo font_info{};
        if (font_offset < 0 || !stbtt_InitFont(&font_info, font_bytes.data(), font_offset)) {
            throw Exceptions::runtime_exception(CE_HERE,
                                                "Unsupported or corrupt font file '" + font_path.string() + "'");
        }

        // Bake the fixed printable range into a growing alpha atlas until every glyph fits.
        std::array<stbtt_bakedchar, font_character_count> baked_characters{};
        int atlas_size = 256;
        std::vector<unsigned char> bitmap;
        while (true) {
            // Retry the whole printable range at double resolution; a
            // partial bake cannot supply stable glyph indices for drawing.
            bitmap.assign(static_cast<std::size_t>(atlas_size) * atlas_size, 0);
            const int result = stbtt_BakeFontBitmap(font_bytes.data(), font_offset, static_cast<float>(font_size),
                                                    bitmap.data(), atlas_size, atlas_size, first_font_character,
                                                    static_cast<int>(font_character_count), baked_characters.data());
            if (result > 0)
                break;
            if (atlas_size == 4096) {
                throw Exceptions::runtime_exception(CE_HERE,
                                                    "Font glyphs do not fit in an atlas: '" + font_path.string() + "'");
            }
            atlas_size *= 2;
        }

        constexpr auto vertex_count = static_cast<std::uint32_t>(font_character_count * VAONumbers::vertices_per_quad);
        constexpr std::size_t vertices_bytes = sizeof(Vertex2D) * vertex_count;
        auto& manager = Mem::ExactMMgr::get();
        auto chunk = manager.checkout_chunk(vertices_bytes, alignof(Vertex2D));
        auto vertices = Mem::make_managed_block<Vertex2D>(manager, std::move(chunk));
        // Build one reusable quad per glyph and retain its advance separately for pen movement.
        std::array<float, font_character_count> advances{};
        for (std::size_t index = 0; index < baked_characters.size(); ++index) {
            float x = 0.0f;
            float y = 0.0f;
            stbtt_aligned_quad quad{};
            stbtt_GetBakedQuad(baked_characters.data(), atlas_size, atlas_size, static_cast<int>(index), &x, &y, &quad,
                               1);
            set_glyph_vertices(vertices.get() + index * VAONumbers::vertices_per_quad, quad);
            advances[index] = x;
        }

        int ascent = 0;
        int descent = 0;
        int line_gap = 0;
        stbtt_GetFontVMetrics(&font_info, &ascent, &descent, &line_gap);
        // Keep line advance in the same pixel scale as the baked glyph quads,
        // so newline motion is independent of individual glyph heights.
        const float scale = stbtt_ScaleForPixelHeight(&font_info, static_cast<float>(font_size));
        const float line_height = std::ceil(static_cast<float>(ascent - descent + line_gap) * scale);
        // The provider copies both transient CPU buffers into backend resources before return.
        auto geometry = provider.upload_geometry(std::move(vertices), vertex_count);
        auto atlas = provider.create_font_atlas(bitmap, PixelSize{static_cast<std::uint32_t>(atlas_size),
                                                                  static_cast<std::uint32_t>(atlas_size)});
        return {std::move(geometry), std::move(atlas), advances, line_height};
    }
}
