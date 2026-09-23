#include <core/resources/asset-management/sprite-mgr.h>

#include <assets/2d/grid-geometry.h>
#include <assets/abstracts/resource-provider.h>
#include <core/resources/asset-management/texture-mgr.h>
#include <core/resources/objects/object-construction.hpp>
#include <internals/exceptions.h>

#include <utility>
#include <algorithm>

namespace CE::Assets {
    void SpriteMgr::load_assets(const std::vector<SpriteDefinition>& definitions, ResourceProvider& provider) {
        bind_provider(provider);
        // Reserve only slots for new IDs; existing assets keep their handles across repeated loads.
        const auto needed = std::count_if(definitions.begin(), definitions.end(), [this](const auto& definition) {
            return !loaded_assets.contains(definition.id());
        });
        auto reservation = reserve<Sprite>(needed);
        std::size_t slot = 0;
        for (std::size_t index = 0; index < definitions.size(); ++index) {
            const auto& definition = definitions[index];
            const auto id = definition.id();
            if (loaded_assets.contains(id)) {
                continue;
            }
            const auto texture = TextureMgr::get().get_asset(definition.texture);
            if (!texture) {
                throw Exceptions::runtime_exception(CE_HERE,
                                                    "Sprite '" + id + "' references an unloaded texture '" +
                                                        definition.texture.string() + "'");
            }
            const auto texture_size = texture->pixel_size();
            if (texture_size.width == 0 || texture_size.height == 0) {
                throw Exceptions::runtime_exception(CE_HERE, "Sprite '" + id + "' has an empty texture");
            }
            // Build CPU vertices from manifest grid/pivot, upload them through the selected backend,
            // and transfer the claimed slot from this reservation into the cache.
            auto geometry = make_grid_geometry(definition.grid, definition.pivot, texture_size);
            auto mesh = provider.upload_geometry(std::move(geometry.vertices), geometry.vertex_count);
            auto asset = reservation.emplace(slot++, SpriteData{.geometry = std::move(mesh),
                                                                 .texture = texture,
                                                                 .definition = definition});
            loaded_assets.emplace(id, asset);
        }
    }
}
