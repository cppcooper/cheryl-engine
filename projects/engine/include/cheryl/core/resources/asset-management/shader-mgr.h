#pragma once

#include <cheryl/export.h>
#include <assets/resources/shader.h>
#include <glm.hpp>
#include <templates/asset-mgr.h>
#include <templates/singleton.h>

namespace CE::Assets::Shaders {
    /** Strong program cache keyed by exact filesystem paths, separate from qualified recipe catalogue IDs. */
    using ProgramStore = CE::Assets::Cache::Store<CE::Rendering::Resources::Program>;
}

namespace CE::Rendering::Resources {
    struct Provider;
}

namespace CE::Assets::Shaders {
    /** Retained linked programs under exact path keys. Load/reload on the active
     * provider/loading owner; borrowed inputs are needed only through the call.
     * Camera/draw state is mutable program access on the backend owner.
     */
    struct CE_TYPE_API ProgramCache final : ProgramStore,
                                            CE::State::Singleton<ProgramCache> {
        ProgramCache() = default;
        ~ProgramCache() override = default;
        // Existing keys skip linking; get_asset() returns a retained program or null.
        CE_API void load(
            const std::filesystem::path& key,
            const std::vector<std::filesystem::path>& stages,
            CE::Rendering::Resources::Provider& provider
        );
        // Build before replacing/inserting one entry; a throw/null program preserves
        // the previous entry. Existing readers retain the previous program generation.
        CE_API void reload(
            const std::filesystem::path& key,
            const std::vector<std::filesystem::path>& stages,
            CE::Rendering::Resources::Provider& provider
        );
    };
}

namespace CE::State {
    template <> CE_API auto Singleton<CE::Assets::Shaders::ProgramCache>::storage() -> Storage&;
}

/**
 * @fn CE::Assets::Shaders::ProgramCache::ProgramCache()
 * @brief Create an empty linked-program cache sharing the global provider/loading-owner policy.
 */

/**
 * @fn CE::Assets::Shaders::ProgramCache::~ProgramCache()
 * @brief Drop cache ownership; retained frames/readers keep their own executable handles.
 */

/**
 * @fn void CE::Assets::Shaders::ProgramCache::load(const std::filesystem::path& key, const std::vector<std::filesystem::path>& stages, CE::Rendering::Resources::Provider& provider)
 * @brief Verify/bind the active loading owner and link only an absent exact key.
 * @param key Exact path cache identity, not a source-file scan request.
 * @param stages Stage paths borrowed through synchronous compilation.
 * @param provider Active backend on its owner/current context.
 * Existing entries skip linking; missing entries use reload() semantics. Provider/file/compile/link/allocation errors propagate.
 */

/**
 * @fn void CE::Assets::Shaders::ProgramCache::reload(const std::filesystem::path& key, const std::vector<std::filesystem::path>& stages, CE::Rendering::Resources::Provider& provider)
 * @brief Link a complete candidate before replacing/inserting one executable generation.
 * @param key Exact cache identity.
 * @param stages Stage files borrowed through the call.
 * @param provider Active loading-owner backend.
 * Null programs throw failed_operation; linking/publication failures preserve the previous entry. Existing readers retain their old handles and mutable program operations still obey backend affinity.
 */
