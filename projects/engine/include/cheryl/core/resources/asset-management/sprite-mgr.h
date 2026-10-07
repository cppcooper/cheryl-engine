#pragma once
#include <templates/asset-mgr.h>
#include <templates/singleton.h>
#include <assets/types/2d/sprite.h>

using SMgr = CE::Assets::AssetMgr<CE::Assets::Sprite, std::string>;
namespace CE::Assets {
    struct ResourceProvider;

    /** Retained namespace:name cache. Load on the provider/loading owner after images;
     * existing IDs stay unchanged and a batch failure can leave earlier entries.
     * Inherited get_asset() returns a strong handle or null under the cache lock.
     */
    struct SpriteMgr final : SMgr,
                             Singleton_CTS<SpriteMgr> {
        SpriteMgr() = default;
        ~SpriteMgr() override = default;
        void load_assets(const std::vector<SpriteDefinition>& definitions, ResourceProvider& provider);
    };
}
