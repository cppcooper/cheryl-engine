#include <assets/submission/draw2d.h>

#include <assets/types/primitives/vertex.h>
#include <ext/matrix_transform.hpp>
#include <internals/exceptions.h>

#include <limits>

namespace CE::Rendering::Submission {
    namespace {
        DrawStyle2D
        image_style(const DrawStyle2D& style, const std::shared_ptr<CE::Rendering::Resources::Image>& image, const Context2D& context) {
            auto result = style;
            if (context.image) {
                if (!image || context.image->key.empty() ||
                    !result.parameters.emplace(context.image->key, CE::Rendering::Parameters::ImageBinding{image, context.image->unit})
                         .second)
                    throw Exceptions::invalid_args(CE_HERE, "Asset image parameter is empty or already supplied");
            }
            return result;
        }

        std::size_t range_start(const std::size_t index, const std::size_t vertices) {
            if (index > std::numeric_limits<std::size_t>::max() / vertices)
                throw Exceptions::invalid_args(CE_HERE, "Asset draw range exceeds addressable geometry");
            return index * vertices;
        }

        DrawPacket2D resolve_range(
            const CE::Assets::Content2D& asset,
            const std::size_t first,
            const std::size_t count,
            const DrawStyle2D& style,
            const Context2D& context
        ) {
            auto selected = style;
            if (!selected.material)
                selected.material = asset.material;
            return resolve_draw_packet(
                asset.geometry, first, count, image_style(selected, asset.texture, context), context.pass, context.parameters,
                context.constraints
            );
        }
    }

    DrawPacket2D resolve_sprite(
        const CE::Assets::Sprites::Asset& sprite,
        const CE::Assets::Grids::CellIndex cell,
        const DrawStyle2D& style,
        const Context2D& context
    ) {
        if (cell >= sprite.definition().grid.cell_count())
            throw Exceptions::invalid_args(CE_HERE, "Sprite submission selects a cell outside its grid");
        return resolve_range(
            sprite, range_start(cell, CE::Rendering::Geometry::vertices_per_strip_quad), CE::Rendering::Geometry::vertices_per_strip_quad,
            style, context
        );
    }

    DrawPacket2D resolve_tile(
        const CE::Assets::Tilesets::Asset& tileset,
        const CE::Assets::Grids::CellIndex cell,
        const DrawStyle2D& style,
        const Context2D& context
    ) {
        if (cell >= tileset.definition().grid.cell_count())
            throw Exceptions::invalid_args(CE_HERE, "Tile submission selects a cell outside its grid");
        return resolve_range(
            tileset, range_start(cell, CE::Rendering::Geometry::vertices_per_strip_quad), CE::Rendering::Geometry::vertices_per_strip_quad,
            style, context
        );
    }

    DrawPacket2D resolve_tile(const CE::Assets::Tilesets::Tile& tile, const DrawStyle2D& style, const Context2D& context) {
        return resolve_range(
            tile, range_start(tile.cell(), CE::Rendering::Geometry::vertices_per_strip_quad),
            CE::Rendering::Geometry::vertices_per_strip_quad, style, context
        );
    }

    DrawPacket2D resolve_tile(const CE::Assets::Tilesets::Animation& animation, const DrawStyle2D& style, const Context2D& context) {
        return resolve_tile(
            CE::Assets::Tilesets::Tile(
                animation.definition().frames.at(animation.index()).cell, animation.geometry, animation.texture, animation.material
            ),
            style, context
        );
    }

    DrawPacket2D resolve_graphic(const CE::Assets::Graphics::Asset& graphic, const DrawStyle2D& style, const Context2D& context) {
        return resolve_range(graphic, 0, CE::Rendering::Geometry::vertices_per_quad, style, context);
    }

    std::vector<DrawPacket2D> resolve_text(
        const CE::Text::Legacy::Font& font,
        const std::string_view text,
        const DrawStyle2D& style,
        const Context2D& context,
        const CE::Text::Legacy::LayoutOptions options
    ) {
        const auto glyphs = font.layout(text, options);
        auto selected = style;
        if (!selected.material)
            selected.material = font.material_handle();
        const auto base = image_style(selected, font.glyph_atlas_handle(), context);
        std::vector<DrawPacket2D> result;
        result.reserve(glyphs.size());
        for (const auto& glyph : glyphs) {
            auto placed = base;
            placed.model_matrix = glm::translate(style.model_matrix, glm::vec3(glyph.x * style.scale, glyph.y * style.scale, 0.0f));
            result.push_back(resolve_draw_packet(
                font.glyph_geometry_handle(), range_start(glyph.index, CE::Rendering::Geometry::vertices_per_quad),
                CE::Rendering::Geometry::vertices_per_quad, placed, context.pass, context.parameters, context.constraints
            ));
        }
        return result;
    }

    std::vector<DrawPacket2D>
    resolve_text(const CE::Text::Rendering::Generation& text, const DrawStyle2D& style, const Context2D& context) {
        if (!context.image || context.image->key.empty() || style.parameters.contains(context.image->key))
            throw Exceptions::invalid_args(CE_HERE, "Unicode text submission needs a page image parameter");
        auto selected = style;
        if (!selected.material)
            selected.material = text.material();
        std::vector<DrawPacket2D> result;
        result.reserve(text.draws().size());
        for (const auto& glyph : text.draws()) {
            const auto& page = text.pages()[glyph.page];
            auto placed = image_style(selected, page.atlas, context);
            placed.model_matrix = glm::translate(style.model_matrix, glm::vec3(glyph.x * style.scale, glyph.y * style.scale, 0));
            result.push_back(resolve_draw_packet(
                page.geometry, range_start(glyph.index, CE::Rendering::Geometry::vertices_per_quad),
                CE::Rendering::Geometry::vertices_per_quad, placed, context.pass, context.parameters, context.constraints
            ));
        }
        return result;
    }

    std::vector<DrawPacket2D>
    resolve_text_runs(const CE::Text::Rendering::Generation& text, const DrawStyle2D& style, const Context2D& context) {
        if (!text.has_positioned_runs())
            throw Exceptions::invalid_args(CE_HERE, "Text run submission requires positioned geometry preparation");
        if (!context.image || context.image->key.empty() || style.parameters.contains(context.image->key))
            throw Exceptions::invalid_args(CE_HERE, "Unicode text submission needs a page image parameter");
        auto material_style = style;
        if (!material_style.material)
            material_style.material = text.material();
        std::vector<DrawPacket2D> result;
        result.reserve(text.runs().size());
        for (const auto& run : text.runs()) {
            const auto selected = image_style(material_style, text.pages()[run.page].atlas, context);
            result.push_back(resolve_draw_packet(
                text.positioned_geometry(), run.first_vertex, run.vertex_count, selected, context.pass, context.parameters,
                context.constraints
            ));
        }
        return result;
    }
}
