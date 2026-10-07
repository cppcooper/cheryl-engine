#pragma once
#include <templates/asset-mgr.h>
#include <templates/singleton.h>
#include <assets/types/2d/tileset.h>

using TSMgr = CE::Assets::AssetMgr<CE::Assets::Tileset, std::string>;
namespace CE::Assets {
    struct ResourceProvider;

    /** Retained namespace:name cache. Load on the provider/loading owner after images;
     * existing IDs stay unchanged and a batch failure can leave earlier entries.
     * Inherited get_asset() returns a strong handle or null under the cache lock.
     */
    struct TilesetMgr final : TSMgr,
                              Singleton_CTS<TilesetMgr> {
        TilesetMgr() = default;
        ~TilesetMgr() override = default;
        void load_assets(const std::vector<TilesetDefinition>& definitions, ResourceProvider& provider);
    };
}
