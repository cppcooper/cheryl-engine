#pragma once
#include <templates/asset-mgr.h>
#include <templates/singleton.h>
#include <assets/types/2d/tileset.h>

namespace CE::Assets::Tilesets {
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
    struct Manager final : Store,
                           CE::State::Singleton<Manager> {
        Manager() = default;
        ~Manager() override = default;
        void load(const std::vector<Definition>& definitions, CE::Rendering::Resources::Provider& provider);
    };
}
