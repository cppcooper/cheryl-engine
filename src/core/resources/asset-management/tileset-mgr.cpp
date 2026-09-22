#include <core/resources/asset-management/tileset-mgr.h>

#include <assets/2d/grid-geometry.h>
#include <core/resources/asset-management/texture-mgr.h>
#include <core/resources/objects/object-construction.hpp>
#include <internals/exceptions.h>


namespace CE::Assets {
    void TilesetMgr::load_assets(const std::vector<TilesetDefinition>& definitions) {
        auto assets = allocate<Tileset>(definitions.size());
        for (std::size_t index = 0; index < definitions.size(); ++index) {
            const auto& definition = definitions[index];
            const auto id = definition.id();
            if (loaded_assets.contains(id)) {
                continue;
            }
            const auto texture = TextureMgr::get().get_asset(definition.texture);
            if (!texture) {
                throw Exceptions::runtime_exception(CE_HERE,
                                                    "Tileset '" + id + "' references an unloaded texture '" +
                                                        definition.texture.string() + "'");
            }
            if (texture->width <= 0 || texture->height <= 0) {
                throw Exceptions::runtime_exception(CE_HERE, "Tileset '" + id + "' has an empty texture");
            }
            const PixelSize texture_size{static_cast<std::uint32_t>(texture->width),
                                         static_cast<std::uint32_t>(texture->height)};
            auto geometry = make_grid_geometry(definition.grid, definition.pivot, texture_size);
            const auto& asset = assets[index];
            Obj::ObjCtor<Tileset>::construct(asset.get(), 1,
                                             TilesetData{.vertices = std::move(geometry.vertices),
                                                         .vertex_count = geometry.vertex_count,
                                                         .texture = texture,
                                                         .definition = definition});
            loaded_assets.emplace(id, asset);
        }
    }
}
