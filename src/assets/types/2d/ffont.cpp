#include <assets/resources/resource-provider.h>
#include <assets/types/2d/ffont.h>
#include <assets/types/primitives/vertex.h>
#include <core/resources/asset-management/texture-mgr.h>
#include <fstream>
#include <internals/exceptions.h>
#include <math/anchor.h>

namespace CE::Assets {
    using namespace VAONumbers;
    constexpr uint16_t num_vertices = num_chars_ffont * vertices_per_quad;

    std::vector<GlyphPlacement2D> FFont::layout(const std::string_view text, const FontLayoutOptions options) const {
        std::vector<GlyphPlacement2D> result;
        result.reserve(text.size());
        float x = 0.0f;
        float y = 0.0f;
        const std::size_t bank = options.alternate_bank ? 128 : 0;
        for (const unsigned char requested : text) {
            if (requested == '\n') {
                x = 0.0f;
                y -= 1.0f / 128.0f; // Preserve the legacy unscaled line advance.
                continue;
            }
            if (requested == '\r')
                continue;
            const auto letter = requested < 32 || requested > 126 ? static_cast<unsigned char>('?') : requested;
            const auto index = bank + letter - 32;
            if (letter != ' ')
                result.push_back({index, x, y});
            x += widths[index] / 128.0f;
        }
        return result;
    }

    void make_vertices(Vertex2D* vertices) {
        for (int idx = 0; idx < num_chars_ffont; ++idx) {
            uint16_t x0 = idx % 16;
            uint16_t y0 = idx / 16;
            math::Anchor::Center(vertices + (idx * vertices_per_quad), 16, 16, 1, 1, x0, y0);
        }
    }

    FFontData FFont::load_ffont(const std::filesystem::path& path, ResourceProvider& provider) {
        std::fstream file(path);
        if (!file.is_open()) {
            // todo: throw
        }
        // todo: make sure we have the file we want
        std::array<short, num_chars_ffont> buffer{};
        file.read(reinterpret_cast<char*>(buffer.data()), buffer.size() * sizeof(short));
        file.close();

        // Convert stored glyph widths for pen movement, then upload the fixed
        // atlas geometry while reusing the already loaded font image.
        std::array<float, num_chars_ffont> widths{};
        for (int idx = 0; idx < num_chars_ffont; ++idx) {
            widths[idx] = static_cast<float>(buffer[idx]);
        }

        auto vertices = std::make_shared<std::array<Vertex2D, num_vertices>>();
        make_vertices(vertices->data());
        auto texture = TextureMgr::get().get_asset("whitefont.png");
        if (!texture)
            throw Exceptions::runtime_exception(CE_HERE, "The legacy font texture is not loaded");
        std::shared_ptr<Vertex2D> verts(vertices, vertices->data());
        auto geometry = provider.upload_geometry(verts, num_vertices, PrimitiveTopology::Triangles);
        return {widths, std::move(geometry), std::move(texture)};
    }
}
