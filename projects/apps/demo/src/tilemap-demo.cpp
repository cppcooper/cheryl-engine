#include "tilemap-demo.h"

#include <core/resources/asset-management/manifest-loader.h>
#include <core/resources/asset-management/texture-mgr.h>
#include <core/resources/asset-management/tileset-mgr.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>

namespace {
    namespace Maps = CE::Tilemaps;
    namespace Submit = CE::Rendering::Submission;
    constexpr Maps::LayerIndex ground{0};
    constexpr Maps::LayerIndex detail{1};
    constexpr std::array<CE::Assets::Grids::CellIndex, 4> targets{0, 1, 2, 3};

    void validate_definition(const CE::Assets::Tilesets::Definition& definition, const std::filesystem::path& root) {
        const auto& grid = definition.grid;
        if (definition.id() != "tileset:tileset" ||
            definition.texture != (root / "graphics/textures/tilesets/tileset.png").lexically_normal() || grid.rows != 1 ||
            grid.columns != 4 || grid.frame.width != 100 || grid.frame.height != 100 || grid.origin.x != 0 || grid.origin.y != 0 ||
            grid.spacing.x != 0 || grid.spacing.y != 0 || definition.pivot.x != 0.0f || definition.pivot.y != 1.0f ||
            !definition.animations.empty())
            throw std::runtime_error("Expected the static 1x4, 100x100 grid with bottom-left pivot {0,1}");
        for (const auto target : targets) {
            const auto rect = grid.cell_rect(target);
            if (rect.x + rect.width > 400 || rect.y + rect.height > 100)
                throw std::runtime_error("Required atlas target is outside the 400x100 image");
        }
    }

    void fill(Maps::Map& map, const Maps::LayerIndex layer, const std::array<std::uint32_t, 6>& ids) {
        for (std::int64_t row = 0; row < 2; ++row)
            for (std::int64_t column = 0; column < 3; ++column)
                map.set(layer, {column, row}, {ids[static_cast<std::size_t>(row * 3 + column)]});
    }
}

DemoTilemaps::DemoTilemaps() {
    // Logical IDs 7..10 select brown/black/grey/red; zero has no binding or draw.
    fill(west_, ground, {7, 8, 9, 10, 0, 7});
    fill(west_, detail, {0, 10, 0, 9, 0, 0});
    fill(east_, ground, {9, 7, 0, 8, 10, 9});
    fill(east_, detail, {10, 0, 10, 0, 7, 0});
}

void DemoTilemaps::load(const std::filesystem::path& root, CE::Rendering::Resources::Provider& provider) {
    const auto source = root / "graphics/definitions/tilesets/tileset.json";
    try {
        const auto manifest = CE::Assets::Manifests::Reader::load(source);
        const auto selected =
            std::ranges::find_if(manifest.tilesets, [](const auto& definition) { return definition.id() == "tileset:tileset"; });
        if (selected == manifest.tilesets.end())
            throw std::runtime_error("Missing required manifest entry tileset:tileset");
        validate_definition(*selected, root);
        const auto decoded = CE::Assets::Images::decode(selected->texture);
        if (decoded.size.width != 400 || decoded.size.height != 100 || decoded.rgba.size() != 400 * 100 * 4)
            throw std::runtime_error("Required atlas must decode to 400x100 RGBA8");
        CE::Assets::Images::Manager::get().upload(selected->texture, decoded, provider);
        auto& tilesets = CE::Assets::Tilesets::Manager::get();
        tilesets.load({*selected}, provider);
        const auto asset = tilesets.get_asset(selected->id());
        if (!asset || !asset->texture)
            throw std::runtime_error("Required tileset/image was not published");
        validate_definition(asset->definition(), root);
        const auto size = asset->texture->pixel_size();
        if (size.width != decoded.size.width || size.height != decoded.size.height)
            throw std::runtime_error("Retained atlas dimensions disagree with the decoded image");
        for (std::size_t i = 0; i < targets.size(); ++i)
            bindings_.add({static_cast<std::uint32_t>(7 + i)}, asset, targets[i]);
        std::cout << "Tilemap display ready: tileset:tileset, decoded 400x100 RGBA8, grid 1x4, pivot {0,1}, targets 0..3\n"
                     "Two fixed 3x2 maps, cells 64x48; west {48,240}, east {328,208}; detail alpha 0.5\n"
                     "Submission: west ground/detail, east ground/detail; ID 7 -> target 0, ID 0 -> empty\n"
                  << std::flush;
    } catch (const std::exception& error) {
        throw std::runtime_error("Required tilemap display atlas 'tileset:tileset' (" + source.string() + "): " + error.what());
    }
}

void DemoTilemaps::write(
    CE::Rendering::PassWriter& pass,
    const CE::Rendering::DrawStyle2D& images,
    const CE::Rendering::Submission::Context2D& context
) const {
    const Submit::TilemapView west_view{.origin = {48, 240}, .cell_size = {64, 48}, .visible = {0, 0, 192, 96}};
    const Submit::TilemapView east_view{.origin = {328, 208}, .cell_size = {64, 48}, .visible = {0, 0, 192, 96}};
    auto overlay = images;
    overlay.alpha = 0.5f;
    // Existing pass writers own authored_order; no implicit layer/Z sorting.
    pass.add(Submit::resolve_tilemap_layer(west_, ground, bindings_, west_view, images, context));
    pass.add(Submit::resolve_tilemap_layer(west_, detail, bindings_, west_view, overlay, context));
    pass.add(Submit::resolve_tilemap_layer(east_, ground, bindings_, east_view, images, context));
    pass.add(Submit::resolve_tilemap_layer(east_, detail, bindings_, east_view, overlay, context));
}
