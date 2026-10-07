#include <assets/types/2d/unicode-text.h>

#include <assets/resources/resource-provider.h>
#include <internals/exceptions.h>
#include "../../../text/font-internal.h"

#include FT_OUTLINE_H

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <new>
#include <optional>
#include <utility>

namespace CE::Assets {
    namespace {
        struct PagePacking {
            std::uint32_t x = 1, y = 1, row_height{}, right = 1, bottom = 1;
        };
        struct QuadLocation {
            std::size_t page{}, index{};
        };

        void check_raster(const FT_Error error) {
            if (error == FT_Err_Out_Of_Memory)
                throw std::bad_alloc{};
            if (error)
                throw Exceptions::runtime_exception(CE_HERE, "Unable to rasterize a text glyph");
        }

        void append_quad(
            std::vector<Vertex2D>& vertices,
            const FT_GlyphSlot glyph,
            const std::uint32_t x,
            const std::uint32_t y,
            const std::uint32_t extent
        ) {
            const auto left = static_cast<float>(glyph->bitmap_left);
            const auto top = static_cast<float>(glyph->bitmap_top);
            const auto right = left + static_cast<float>(glyph->bitmap.width);
            const auto bottom = top - static_cast<float>(glyph->bitmap.rows);
            const auto u0 = static_cast<float>(x) / static_cast<float>(extent);
            const auto v0 = static_cast<float>(y) / static_cast<float>(extent);
            const auto u1 = static_cast<float>(x + glyph->bitmap.width) / static_cast<float>(extent);
            const auto v1 = static_cast<float>(y + glyph->bitmap.rows) / static_cast<float>(extent);
            const Quad quad{{{{left, bottom, 0, u0, v1}, {right, bottom, 0, u1, v1}, {right, top, 0, u1, v0},
                {left, bottom, 0, u0, v1}, {right, top, 0, u1, v0}, {left, top, 0, u0, v0}}}};
            vertices.insert(vertices.end(), quad.vertices.begin(), quad.vertices.end());
        }

        void crop_page(PreparedTextPage& page, const PagePacking& packing, const std::uint32_t extent) {
            const auto width = packing.right + 1;
            const auto height = packing.bottom + 1;
            std::vector<unsigned char> cropped(static_cast<std::size_t>(width) * height);
            for (std::uint32_t row = 0; row < height; ++row) {
                std::copy_n(page.alpha.data() + static_cast<std::size_t>(row) * extent, width,
                    cropped.data() + static_cast<std::size_t>(row) * width);
            }
            page.alpha = std::move(cropped);
            page.size = {width, height};
            const auto x_scale = static_cast<float>(extent) / static_cast<float>(width);
            const auto y_scale = static_cast<float>(extent) / static_cast<float>(height);
            for (auto& vertex : page.vertices) {
                vertex.u *= x_scale;
                vertex.v *= y_scale;
            }
        }
    }

    PreparedText::PreparedText(Text::ShapedText layout)
    : layout_(std::move(layout)) {}

    RenderedText::RenderedText(
        Text::ShapedText layout,
        std::vector<TextPageResources> pages,
        std::vector<TextGlyphDraw> draws
    )
    : layout_(std::move(layout)), pages_(std::move(pages)), draws_(std::move(draws)) {}

    PreparedText prepare_text(Text::ShapedText layout, const TextRasterOptions options) {
        const auto extent = options.maximum_page_extent;
        if (extent < 4 || extent > 4096)
            throw Exceptions::invalid_args(CE_HERE, "Text page extent must be between 4 and 4096 pixels");
        PreparedText result(std::move(layout));
        const auto& fonts = Text::Detail::FontAccess::data(result.layout_.fonts());
        auto library = Text::Detail::open_freetype();
        std::map<std::size_t, Text::Detail::FreeTypeFace> faces;
        std::map<Text::GlyphId, std::optional<QuadLocation>> admitted;
        std::vector<PagePacking> packing;
        for (const auto& placed : result.layout_.glyphs()) {
            auto found = admitted.find(placed.id);
            if (found == admitted.end()) {
                if (placed.id.face >= fonts.faces.size() || placed.id.glyph >= fonts.info[placed.id.face].glyph_count)
                    throw Exceptions::runtime_exception(CE_HERE, "Text selects a glyph outside its retained font");
                auto face = faces.find(placed.id.face);
                if (face == faces.end()) {
                    face = faces.emplace(placed.id.face, Text::Detail::open_face(
                        library.get(), fonts.faces[placed.id.face], result.layout_.options().pixel_height)).first;
                }
                check_raster(FT_Load_Glyph(face->second.get(), placed.id.glyph, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP));
                const auto glyph = face->second->glyph;
                if (glyph->format != FT_GLYPH_FORMAT_OUTLINE)
                    throw Exceptions::runtime_exception(CE_HERE, "Text rasterization requires grayscale outline glyphs");
                FT_BBox bounds{};
                FT_Outline_Get_CBox(&glyph->outline, &bounds);
                const auto width = std::ceil(static_cast<double>(bounds.xMax) / 64) - std::floor(static_cast<double>(bounds.xMin) / 64);
                const auto height = std::ceil(static_cast<double>(bounds.yMax) / 64) - std::floor(static_cast<double>(bounds.yMin) / 64);
                if (width > extent - 2 || height > extent - 2)
                    throw Exceptions::runtime_exception(CE_HERE, "A text glyph exceeds its padded page extent");
                check_raster(FT_Render_Glyph(glyph, FT_RENDER_MODE_NORMAL));
                const auto& bitmap = glyph->bitmap;
                if (!bitmap.width || !bitmap.rows) {
                    found = admitted.emplace(placed.id, std::nullopt).first;
                } else {
                    if (bitmap.width > extent - 2 || bitmap.rows > extent - 2)
                        throw Exceptions::runtime_exception(CE_HERE, "A rasterized glyph exceeds its padded page extent");
                    const auto pitch = static_cast<std::int64_t>(bitmap.pitch);
                    const auto stride = pitch < 0 ? -pitch : pitch;
                    if (!bitmap.buffer || bitmap.pixel_mode != FT_PIXEL_MODE_GRAY || bitmap.num_grays < 2 || stride < bitmap.width ||
                        !std::in_range<std::ptrdiff_t>(pitch * static_cast<std::int64_t>(bitmap.rows - 1)))
                        throw Exceptions::runtime_exception(CE_HERE, "Unsupported grayscale glyph bitmap");
                    if (!packing.empty() && packing.back().x + bitmap.width + 1 > extent) {
                        auto& current = packing.back();
                        current.x = 1;
                        current.y += current.row_height + 1;
                        current.row_height = 0;
                    }
                    if (packing.empty() || packing.back().y + bitmap.rows + 1 > extent) {
                        result.pages_.push_back({{extent, extent},
                            std::vector<unsigned char>(static_cast<std::size_t>(extent) * extent), {}});
                        packing.emplace_back();
                    }
                    auto& current = packing.back();
                    auto& page = result.pages_.back();
                    const QuadLocation location{result.pages_.size() - 1, page.vertices.size() / VAONumbers::vertices_per_quad};
                    for (unsigned row = 0; row < bitmap.rows; ++row) {
                        const auto* source = bitmap.buffer + static_cast<std::ptrdiff_t>(pitch * row);
                        auto* destination = page.alpha.data() + static_cast<std::size_t>(current.y + row) * extent + current.x;
                        for (unsigned column = 0; column < bitmap.width; ++column) {
                            const auto alpha = static_cast<unsigned>(source[column]) * 255 / (bitmap.num_grays - 1);
                            destination[column] = static_cast<unsigned char>(alpha);
                        }
                    }
                    append_quad(page.vertices, glyph, current.x, current.y, extent);
                    current.right = std::max(current.right, current.x + bitmap.width);
                    current.bottom = std::max(current.bottom, current.y + bitmap.rows);
                    current.x += bitmap.width + 1;
                    current.row_height = std::max(current.row_height, bitmap.rows);
                    found = admitted.emplace(placed.id, location).first;
                }
            }
            if (found->second)
                result.draws_.push_back({found->second->page, found->second->index, placed.x, placed.y});
        }
        for (std::size_t index = 0; index < result.pages_.size(); ++index)
            crop_page(result.pages_[index], packing[index], extent);
        return result;
    }

    RenderedText upload_text(PreparedText prepared, ResourceProvider& provider) {
        std::vector<TextPageResources> pages;
        pages.reserve(prepared.pages_.size());
        for (const auto& page : prepared.pages_) {
            auto geometry = provider.upload_geometry(page.vertices, PrimitiveTopology::Triangles);
            auto atlas = provider.create_font_atlas(page.alpha, page.size);
            const auto size = atlas ? atlas->pixel_size() : PixelSize{};
            if (!geometry || !atlas || geometry->vertex_count() != page.vertices.size() ||
                geometry->vertex_layout() != VertexLayout2D::Position3UV2 || geometry->topology() != PrimitiveTopology::Triangles ||
                size.width != page.size.width || size.height != page.size.height)
                throw Exceptions::failed_operation(CE_HERE, "Text upload returned mismatched geometry or atlas resources");
            pages.push_back({std::move(geometry), std::move(atlas)});
        }
        return RenderedText(std::move(prepared.layout_), std::move(pages), std::move(prepared.draws_));
    }
}
