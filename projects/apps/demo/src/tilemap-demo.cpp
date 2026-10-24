#include "tilemap-demo.h"

#include <core/resources/asset-management/manifest-loader.h>
#include <core/resources/asset-management/texture-mgr.h>
#include <core/resources/asset-management/tileset-mgr.h>
#include <internals/exceptions.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>

namespace {
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
                     "Two 3x2 maps, cells 64x48; west {48,240}, east {328,208}; detail alpha 0.5\n"
                     "Submission: west ground/detail, east ground/detail; ID 7 -> target 0, ID 0 -> empty\n"
                     "Editor: 1/2 west/east, G/T ground/detail, 3..6 brown/black/grey/red\n"
                     "V literal (initial), B semantic terrain; terrain 3..6 choose opaque labels 1..4\n"
                     "Modes keep separate maps/brushes; semantic layers start empty; mask N/E/S/W -> target mask%4 -> IDs 7..10\n"
                     "Terrain click runs publish atomically (32 requests, 6 affected cells, 256 work units); no held brush\n"
                     "Left paint, right erase, middle sample (including empty); WASD pan 32 world units per press\n"
                     "Wheel zoom at pointer (4 notches doubles); R reset view; initial west/ground/brown\n"
                     "Literal only: F8 SAVE / F9 LOAD selected document; --tilemap-document-dir PATH enables west.json/east.json\n"
                  << std::flush;
    } catch (const std::exception& error) {
        throw std::runtime_error("Required tilemap display atlas 'tileset:tileset' (" + source.string() + "): " + error.what());
    }
}

void DemoTilemaps::update(const std::span<const CE::Input::Events::Record> records, const DemoTilemapEditor::Sizes sizes) {
    try {
        editor_.consume(records, sizes);
    } catch (const CE::Exceptions::bad_request& error) {
        // Engine preserved the refused run. Earlier boundary-completed runs
        // remain; report the rejection without aborting ordinary demo updates.
        std::cerr << "Tilemap input/action rejected (remaining tick records dropped): " << error.what() << '\n';
    } catch (const CE::Maps::Error& error) {
        std::cerr << "Tilemap document storage rejected (remaining tick records dropped): " << error.what() << '\n';
    } catch (const WorldProof::Error& error) {
        std::cerr << "Tilemap document conversion rejected (remaining tick records dropped): " << error.what() << '\n';
    }
}

void DemoTilemaps::write(CE::Rendering::FrameWriter& frame, const CE::Rendering::DrawStyle2D& images) const {
    editor_.write(frame, bindings_, images);
}
