#include <core/resources/asset-management/tileset-mgr.h>

#include <assets/2d/grid-geometry.h>
#include <assets/abstracts/resource-provider.h>
#include <core/resources/asset-management/texture-mgr.h>
#include <core/resources/objects/object-construction.hpp>
#include <internals/exceptions.h>

#include <utility>

namespace CE::Assets {
    void TilesetMgr::load_assets(const std::vector<TilesetDefinition>& definitions, ResourceProvider& provider) {
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
            const auto texture_size = texture->pixel_size();
            if (texture_size.width == 0 || texture_size.height == 0) {
                throw Exceptions::runtime_exception(CE_HERE, "Tileset '" + id + "' has an empty texture");
            }
            auto geometry = make_grid_geometry(definition.grid, definition.pivot, texture_size);
            auto mesh = provider.upload_geometry(std::move(geometry.vertices), geometry.vertex_count);
            const auto& asset = assets[index];
            Obj::ObjCtor<Tileset>::construct(asset.get(), 1,
                                             TilesetData{.geometry = std::move(mesh),
                                                         .texture = texture,
                                                         .definition = definition});
            loaded_assets.emplace(id, asset);
        }
    }
}
