#pragma once

#include <cheryl/export.h>
#include <templates/asset-mgr.h>
#include <templates/singleton.h>
#include <assets/types/2d/tileset.h>

namespace CE::Assets::Tilesets {
    /** Strong tilesets cache keyed by exact qualified namespace:name identities. */
    using Store = CE::Assets::Cache::Store<CE::Assets::Tilesets::Asset, std::string>;
}

namespace CE::Rendering::Resources {
    struct Provider;
}

namespace CE::Assets::Tilesets {
    /** Retained namespace:name cache. Load on the provider/loading owner after images;
     * existing IDs stay unchanged and a batch failure can leave earlier entries.
     * Inherited get_asset() returns a strong handle or null under the cache lock.
     */
    struct CE_TYPE_API Manager final : Store,
                                       CE::State::Singleton<Manager> {
        Manager() = default;
        ~Manager() override = default;
        CE_API void load(const std::vector<Definition>& definitions, CE::Rendering::Resources::Provider& provider);
    };
}

namespace CE::State {
    template <> CE_API auto Singleton<CE::Assets::Tilesets::Manager>::storage() -> Storage&;
}

/**
 * @fn CE::Assets::Tilesets::Manager::Manager()
 * @brief Create an empty tilesets cache sharing the active provider/loading-owner domain.
 */

/**
 * @fn CE::Assets::Tilesets::Manager::~Manager()
 * @brief Drop cache ownership; retained tilesets values/frames keep their own resources.
 */

/**
 * @fn void CE::Assets::Tilesets::Manager::load(const std::vector<Definition>& definitions, CE::Rendering::Resources::Provider& provider)
 * @brief Build and publish absent qualified tilesets IDs after their textures and explicit materials are loaded.
 * @param definitions Stable CPU definitions with referenced grid/animation metadata.
 * @param provider Active loading-owner backend/current context for geometry/default-material upload.
 * Existing IDs retain their original handles without rebuilding. Explicit shader selections are retained; absent selections use the backend TexturedStrip default. Invalid definitions, missing image/materials, backend or allocation failures may leave earlier entries published. Reads retain immutable resources across cache clears.
 * @see [Asset values](cheryl-guide:assets/asset-values-and-playback)
 */
