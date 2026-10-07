#include <assets/submission/draw2d.h>

#include <assets/types/primitives/vertex.h>
#include <ext/matrix_transform.hpp>
#include <internals/exceptions.h>

#include <limits>

namespace CE::Assets {
    namespace {
        RenderAPIs::DrawStyle2D
        image_style(const RenderAPIs::DrawStyle2D& style, const std::shared_ptr<Image>& image, const SubmissionContext2D& context) {
            auto result = style;
            if (context.image) {
                if (!image || context.image->key.empty() ||
                    !result.parameters.emplace(context.image->key, ImageBinding{image, context.image->unit}).second)
                    throw Exceptions::invalid_args(CE_HERE, "Asset image parameter is empty or already supplied");
            }
            return result;
        }

        std::size_t range_start(const std::size_t index, const std::size_t vertices) {
            if (index > std::numeric_limits<std::size_t>::max() / vertices)
                throw Exceptions::invalid_args(CE_HERE, "Asset draw range exceeds addressable geometry");
            return index * vertices;
        }

        RenderAPIs::DrawPacket2D resolve_range(
            const Asset2D& asset,
            const std::size_t first,
            const std::size_t count,
            const RenderAPIs::DrawStyle2D& style,
            const SubmissionContext2D& context
        ) {
            return RenderAPIs::resolve_draw_packet(
                asset.geometry, first, count, image_style(style, asset.texture, context), context.pass, context.parameters,
                context.constraints
            );
        }
    }

    RenderAPIs::DrawPacket2D
    resolve_sprite(const Sprite& sprite, const CellIndex cell, const RenderAPIs::DrawStyle2D& style, const SubmissionContext2D& context) {
        if (cell >= sprite.definition().grid.cell_count())
            throw Exceptions::invalid_args(CE_HERE, "Sprite submission selects a cell outside its grid");
        return resolve_range(
            sprite, range_start(cell, VAONumbers::vertices_per_strip_quad), VAONumbers::vertices_per_strip_quad, style, context
        );
    }

    RenderAPIs::DrawPacket2D
    resolve_tile(const Tileset& tileset, const CellIndex cell, const RenderAPIs::DrawStyle2D& style, const SubmissionContext2D& context) {
        if (cell >= tileset.definition().grid.cell_count())
            throw Exceptions::invalid_args(CE_HERE, "Tile submission selects a cell outside its grid");
        return resolve_range(
            tileset, range_start(cell, VAONumbers::vertices_per_strip_quad), VAONumbers::vertices_per_strip_quad, style, context
        );
    }

    RenderAPIs::DrawPacket2D resolve_tile(const Tile& tile, const RenderAPIs::DrawStyle2D& style, const SubmissionContext2D& context) {
        const Asset2D asset(tile.geometry, tile.texture);
        return resolve_range(
            asset, range_start(tile.cell(), VAONumbers::vertices_per_strip_quad), VAONumbers::vertices_per_strip_quad, style, context
        );
    }

    RenderAPIs::DrawPacket2D
    resolve_tile(const TileAnimation& animation, const RenderAPIs::DrawStyle2D& style, const SubmissionContext2D& context) {
        return resolve_tile(
            Tile(animation.definition().frames.at(animation.index()).cell, animation.geometry, animation.texture), style, context
        );
    }

    RenderAPIs::DrawPacket2D
    resolve_graphic(const Graphic& graphic, const RenderAPIs::DrawStyle2D& style, const SubmissionContext2D& context) {
        return resolve_range(graphic, 0, VAONumbers::vertices_per_quad, style, context);
    }

    std::vector<RenderAPIs::DrawPacket2D> resolve_text(
        const Font& font,
        const std::string_view text,
        const RenderAPIs::DrawStyle2D& style,
        const SubmissionContext2D& context,
        const FontLayoutOptions options
    ) {
        const auto glyphs = font.layout(text, options);
        const auto base = image_style(style, font.glyph_atlas_handle(), context);
        std::vector<RenderAPIs::DrawPacket2D> result;
        result.reserve(glyphs.size());
        for (const auto& glyph : glyphs) {
            auto placed = base;
            placed.model_matrix = glm::translate(style.model_matrix, glm::vec3(glyph.x * style.scale, glyph.y * style.scale, 0.0f));
            result.push_back(
                RenderAPIs::resolve_draw_packet(
                    font.glyph_geometry_handle(), range_start(glyph.index, VAONumbers::vertices_per_quad), VAONumbers::vertices_per_quad,
                    placed, context.pass, context.parameters, context.constraints
                )
            );
        }
        return result;
    }

    std::vector<RenderAPIs::DrawPacket2D>
    resolve_text(const RenderedText& text, const RenderAPIs::DrawStyle2D& style, const SubmissionContext2D& context) {
        if (!context.image || context.image->key.empty() || style.parameters.contains(context.image->key))
            throw Exceptions::invalid_args(CE_HERE, "Unicode text submission needs a page image parameter");
        std::vector<RenderAPIs::DrawPacket2D> result;
        result.reserve(text.draws().size());
        for (const auto& glyph : text.draws()) {
            const auto& page = text.pages()[glyph.page];
            auto placed = image_style(style, page.atlas, context);
            placed.model_matrix = glm::translate(style.model_matrix, glm::vec3(glyph.x * style.scale, glyph.y * style.scale, 0));
            result.push_back(RenderAPIs::resolve_draw_packet(
                page.geometry, range_start(glyph.index, VAONumbers::vertices_per_quad), VAONumbers::vertices_per_quad,
                placed, context.pass, context.parameters, context.constraints
            ));
        }
        return result;
    }
}
