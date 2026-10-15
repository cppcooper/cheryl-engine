#pragma once
#include <templates/asset-mgr.h>
#include <templates/singleton.h>
#include <assets/types/2d/sprite.h>

#include <map>
#include <string_view>

namespace CE::Assets::Sprites {
    using Store = CE::Assets::Cache::Store<CE::Assets::Sprites::Asset, std::string>;
}

namespace CE::Rendering::Resources {
    struct Provider;
}

namespace CE::Assets::Sprites {
    /** Retained namespace:name cache. Load on the provider/loading owner after images;
     * existing IDs stay unchanged and a batch failure can leave earlier entries.
     * Inherited get_asset() returns a strong handle or null under the cache lock.
     */
    struct Manager final : Store,
                           CE::State::Singleton<Manager> {
        Manager() = default;
        ~Manager() override = default;
        using Store::get_asset;
        /** Look up an exact namespace/name without loading assets.
         * @param name_space Asset definition namespace, copied into the lookup key.
         * @param name Local asset name, copied into the lookup key.
         * @return Retained sprite handle, or null if absent; it survives cache clearing.
         */
        [[nodiscard]] Handle get_asset(std::string_view name_space, std::string_view name) const;
        /** Snapshot published sprites in one exact namespace without loading entries.
         * @param name_space Namespace matched against each retained definition.
         * @return Owned map keyed by local name in lexical order, empty if absent;
         * copied handles survive cache clears.
         */
        [[nodiscard]] std::map<std::string, Handle, std::less<>> get_assets(std::string_view name_space) const;
        /** Construct and publish sprites whose IDs are not already cached.
         * Missing textures/materials, invalid definitions or upload failures can
         * leave earlier sprites published; existing IDs retain their original handles.
         * @param definitions Stable sprite definitions; referenced textures and
         * optional shader materials must already be loaded.
         * @param provider Active backend used on its loading owner/current context.
         */
        void load(const std::vector<Definition>& definitions, CE::Rendering::Resources::Provider& provider);
    };
}
