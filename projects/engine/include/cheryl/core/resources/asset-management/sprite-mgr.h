#pragma once

#include <cheryl/export.h>
#include <templates/asset-mgr.h>
#include <templates/singleton.h>
#include <assets/types/2d/sprite.h>

#include <map>
#include <string_view>

namespace CE::Assets::Sprites {
    /** Strong sprites cache keyed by exact qualified namespace:name identities. */
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
    struct CE_TYPE_API Manager final : Store,
                                       CE::State::Singleton<Manager> {
        Manager() = default;
        ~Manager() override = default;
        using Store::get_asset;
        [[nodiscard]] CE_API Handle get_asset(std::string_view name_space, std::string_view name) const;
        [[nodiscard]] CE_API std::map<std::string, Handle, std::less<>> get_assets(std::string_view name_space) const;
        CE_API void load(const std::vector<Definition>& definitions, CE::Rendering::Resources::Provider& provider);
    };
}

namespace CE::State {
    template <> CE_API auto Singleton<CE::Assets::Sprites::Manager>::storage() -> Storage&;
}

/**
 * @fn CE::Assets::Sprites::Manager::Manager()
 * @brief Create an empty sprites cache sharing the active provider/loading-owner domain.
 */

/**
 * @fn CE::Assets::Sprites::Manager::~Manager()
 * @brief Drop cache ownership; retained sprites values/frames keep their own resources.
 */

/**
 * @fn void CE::Assets::Sprites::Manager::load(const std::vector<Definition>& definitions, CE::Rendering::Resources::Provider& provider)
 * @brief Build and publish absent qualified sprites IDs after their textures and explicit materials are loaded.
 * @param definitions Stable CPU definitions with referenced grid/animation metadata.
 * @param provider Active loading-owner backend/current context for geometry/default-material upload.
 * Existing IDs retain their original handles without rebuilding. Explicit shader selections are retained; absent selections use the backend TexturedStrip default. Invalid definitions, missing image/materials, backend or allocation failures may leave earlier entries published. Reads retain immutable resources across cache clears.
 * @see [Asset values](cheryl-guide:assets/asset-values-and-playback)
 */

/**
 * @fn CE::Assets::Sprites::Manager::Handle CE::Assets::Sprites::Manager::get_asset(std::string_view name_space, std::string_view name) const
 * @brief Look up one exact namespace/local-name pair without loading.
 * @param name_space Exact definition namespace, copied into the qualified lookup key.
 * @param name Exact local asset name.
 * @return Strong retained handle, or null when absent; shared cache locking protects lookup and later clears do not invalidate the returned owner.
 */

/**
 * @fn auto CE::Assets::Sprites::Manager::get_assets(std::string_view name_space) const -> std::map<std::string, Handle, std::less<>>
 * @brief Copy published sprites from one exact definition namespace under the cache read lock.
 * @param name_space Namespace matched against each retained definition.
 * @return Owned local-name map in lexical order, empty if absent; copied handles survive later clearing. No native work or loading occurs.
 */
