#include <core/resources/asset-management/tileset-mgr.h>

#include <assets/geometry/grid-geometry.h>
#include <assets/resources/resource-provider.h>
#include <core/resources/asset-management/texture-mgr.h>
#include <core/resources/asset-management/shader-asset-mgr.h>
#include <core/resources/objects/object-construction.hpp>
#include <internals/exceptions.h>

#include <algorithm>
#include <utility>

namespace CE::Assets {
    void TilesetMgr::load_assets(const std::vector<TilesetDefinition>& definitions, ResourceProvider& provider) {
        bind_provider(provider);
        // Reserve for new IDs only; a repeated load does not replace a cached tileset.
        std::vector<const TilesetDefinition*> pending;
        for (const auto& definition : definitions)
            if (!contains(definition.id()))
                pending.push_back(&definition);
        auto reservation = reserve<Tileset>(pending.size());
        std::size_t slot = 0;
        for (const auto* selected : pending) {
            const auto& definition = *selected;
            const auto id = definition.id();
            if (contains(id)) {
                continue;
            }
            const auto texture = TextureMgr::get().get_asset(definition.texture);
            if (!texture) {
                throw Exceptions::runtime_exception(
                    CE_HERE, "Tileset '" + id + "' references an unloaded texture '" + definition.texture.string() + "'"
                );
            }
            const auto texture_size = texture->pixel_size();
            if (texture_size.width == 0 || texture_size.height == 0) {
                throw Exceptions::runtime_exception(CE_HERE, "Tileset '" + id + "' has an empty texture");
            }
            const auto material = definition.shader ? ShaderAssetMgr::get().get_material(*definition.shader) : nullptr;
            if (definition.shader && !material)
                throw Exceptions::runtime_exception(CE_HERE, "Tileset '" + id + "' references an unloaded shader '" + *definition.shader + "'");
            // Translate each grid cell into vertices, upload once, then retain geometry and
            // unconsumed animation/autotile metadata in the cached tileset.
            auto geometry = make_grid_geometry(definition.grid, definition.pivot, texture_size);
            auto mesh = provider.upload_geometry(std::move(geometry.vertices), geometry.vertex_count, PrimitiveTopology::TriangleStrip);
            if (material) {
                PassConstraints2D unconstrained;
                unconstrained.depth.reset();
                material->definition().pipeline->validate_draw(*mesh, 0, mesh->vertex_count(), unconstrained);
            }
            auto asset =
                reservation.emplace(slot++, TilesetData{.geometry = std::move(mesh), .texture = texture, .definition = definition, .material = material});
            publish_asset(id, std::move(asset));
        }
    }
}
