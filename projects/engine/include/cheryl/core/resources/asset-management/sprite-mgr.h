#pragma once
#include <templates/asset-mgr.h>
#include <templates/singleton.h>
#include <assets/types/2d/sprite.h>

#include <map>
#include <string_view>

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
        using SMgr::get_asset;
        // Exact namespace/name lookup; missing returns null without loading assets.
        [[nodiscard]] spointer get_asset(std::string_view name_space, std::string_view name) const;
        // Snapshot of currently published sprites in this exact namespace, keyed by
        // local name in lexical order. Missing namespaces return an empty map;
        // copied handles survive cache clears. This does not load missing entries.
        [[nodiscard]] std::map<std::string, spointer, std::less<>> get_assets(std::string_view name_space) const;
        void load_assets(const std::vector<SpriteDefinition>& definitions, ResourceProvider& provider);
    };
}
