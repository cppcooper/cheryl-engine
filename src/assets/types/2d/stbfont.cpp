#include <assets/resources/resource-provider.h>
#include <assets/types/2d/stbfont.h>
#include <assets/types/primitives/vertex.h>
#include "font-upload-internal.h"
#include "font-bake-internal.h"
#include "font-stb-allocation-internal.h"

#include <cstddef>
#include <cstdlib>
#include <limits>
#include <memory>
#include <memory_resource>
#include <new>

namespace {
    // stb's C-style cleanup does not run when allocation throws. Track every
    // live block until the bake returns so an exception can release the whole
    // rasterization attempt, including its current glyph's scratch storage.
    class StbAllocationScope {
        struct alignas(
            std::max_align_t
        ) Block {
            StbAllocationScope* owner;
            Block* previous;
            Block* next;
            std::size_t bytes;
        };

        std::pmr::memory_resource& memory_;
        Block* head_ = nullptr;
        StbAllocationScope* previous_;
        static thread_local StbAllocationScope* current_;

        void release(
            Block* block
        ) noexcept {
            if (block->previous)
                block->previous->next = block->next;
            else
                head_ = block->next;
            if (block->next)
                block->next->previous = block->previous;
            memory_.deallocate(block, block->bytes, alignof(Block));
        }

    public:
        explicit StbAllocationScope(
            std::pmr::memory_resource& memory
        )
        : memory_(memory), previous_(current_) {
            current_ = this;
        }
        ~StbAllocationScope() {
            while (head_)
                release(head_);
            current_ = previous_;
        }
        StbAllocationScope(
            const StbAllocationScope&
        ) = delete;
        StbAllocationScope& operator=(
            const StbAllocationScope&
        ) = delete;

        static void* allocate(
            const std::size_t bytes
        ) {
            if (!current_)
                return std::malloc(bytes);
            if (bytes > std::numeric_limits<std::size_t>::max() - sizeof(Block))
                throw std::bad_alloc{};
            const auto total = sizeof(Block) + bytes;
            auto* block = static_cast<Block*>(current_->memory_.allocate(total, alignof(Block)));
            std::construct_at(block, Block{current_, nullptr, current_->head_, total});
            if (current_->head_)
                current_->head_->previous = block;
            current_->head_ = block;
            return block + 1;
        }

        static void free(
            void* pointer
        ) noexcept {
            if (!pointer)
                return;
            if (!current_) {
                std::free(pointer);
                return;
            }
            auto* block = static_cast<Block*>(pointer) - 1;
            block->owner->release(block);
        }
    };

    thread_local StbAllocationScope* StbAllocationScope::current_ = nullptr;
}

#define STBTT_malloc(bytes, userdata) StbAllocationScope::allocate(bytes)
#define STBTT_free(pointer, userdata) StbAllocationScope::free(pointer)
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include <core/resources/memory.h>
#include <core/resources/memory/managed-block.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <internals/exceptions.h>
#include <utility>
#include <vector>

namespace CE::Assets {
    namespace {
        std::vector<unsigned char> read_font_file(
            const std::filesystem::path& path
        ) {
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

        void set_glyph_vertices(
            Vertex2D* vertices,
            const stbtt_aligned_quad& quad
        ) {
            // Flip stb's downward-positive glyph Y into the engine's upward-positive local space;
            // each glyph keeps the standalone quad's two independent triangles.
            const float left = quad.x0;
            const float right = quad.x1;
            const float bottom = -quad.y1;
            const float top = -quad.y0;
            Quad glyph{};
            glyph.vertices[0] = {left, bottom, 0.0f, quad.s0, quad.t1};
            glyph.vertices[1] = {right, bottom, 0.0f, quad.s1, quad.t1};
            glyph.vertices[2] = {right, top, 0.0f, quad.s1, quad.t0};
            glyph.vertices[3] = glyph.vertices[0];
            glyph.vertices[4] = glyph.vertices[2];
            glyph.vertices[5] = {left, top, 0.0f, quad.s0, quad.t0};
            std::copy(glyph.vertices.begin(), glyph.vertices.end(), vertices);
        }
    }

    STBFontData FontDetail::upload_baked_font(
        ResourceProvider& provider,
        std::shared_ptr<Vertex2D> vertices,
        const std::span<const unsigned char> alpha,
        const PixelSize atlas_size,
        const std::array<float, font_character_count>& advances,
        const float line_height
    ) {
        constexpr auto vertex_count = static_cast<std::uint32_t>(font_character_count * VAONumbers::vertices_per_quad);
        auto geometry = provider.upload_geometry(std::move(vertices), vertex_count, PrimitiveTopology::Triangles);
        auto atlas = provider.create_font_atlas(alpha, atlas_size);
        return {std::move(geometry), std::move(atlas), advances, line_height};
    }

    STBFont::STBFont(
        STBFontData data
    )
    : Font({data.geometry, data.texture}), advances_(data.advances), line_height_(data.line_height) {
        if (!geometry || !texture)
            throw Exceptions::invalid_args(CE_HERE, "A font needs glyph geometry and an atlas");
    }

    std::vector<GlyphPlacement2D> STBFont::layout(
        const std::string_view text,
        const FontLayoutOptions options
    ) const {
        if (options.alternate_bank)
            throw Exceptions::invalid_args(CE_HERE, "STB fonts do not contain an alternate glyph bank");
        std::vector<GlyphPlacement2D> result;
        result.reserve(text.size());
        for_each_glyph(text, [&](const std::size_t index, const float x, const float y) { result.push_back({index, x, y}); });
        return result;
    }

    STBFontData STBFont::load_font(
        const std::filesystem::path& font_path,
        const int font_size,
        ResourceProvider& provider
    ) {
        return FontDetail::load_font_with_resource(font_path, font_size, provider, *std::pmr::new_delete_resource());
    }

    STBFontData FontDetail::load_font_with_resource(
        const std::filesystem::path& font_path,
        const int font_size,
        ResourceProvider& provider,
        std::pmr::memory_resource& memory
    ) {
        if (font_size <= 0)
            throw Exceptions::invalid_args(CE_HERE, "Font size must be positive");
        const auto font_bytes = read_font_file(font_path);
        const int font_offset = stbtt_GetFontOffsetForIndex(font_bytes.data(), 0);
        stbtt_fontinfo font_info{};
        if (font_offset < 0 || !stbtt_InitFont(&font_info, font_bytes.data(), font_offset)) {
            throw Exceptions::runtime_exception(CE_HERE, "Unsupported or corrupt font file '" + font_path.string() + "'");
        }

        // Bake the fixed printable range into a growing alpha atlas until every glyph fits.
        std::array<stbtt_bakedchar, font_character_count> baked_characters{};
        auto atlas = [&] {
            StbAllocationScope allocations(memory);
            return FontDetail::bake_font_atlas(font_path, [&](const std::span<unsigned char> pixels, const int size) {
                return stbtt_BakeFontBitmap(font_bytes.data(), font_offset, static_cast<float>(font_size), pixels.data(), size, size,
                    first_font_character, static_cast<int>(font_character_count), baked_characters.data());
            });
        }();
        const int atlas_size = atlas.size;

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
            stbtt_GetBakedQuad(baked_characters.data(), atlas_size, atlas_size, static_cast<int>(index), &x, &y, &quad, 1);
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
        return FontDetail::upload_baked_font(provider, std::move(vertices), atlas.pixels,
            PixelSize{static_cast<std::uint32_t>(atlas_size), static_cast<std::uint32_t>(atlas_size)}, advances, line_height);
    }
}
