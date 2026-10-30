#pragma once

#include <cheryl/export.h>

#include <assets/resources/pipeline.h>
#include <templates/asset-mgr.h>
#include <templates/singleton.h>

#include <functional>
#include <filesystem>
#include <memory>

namespace CE::Rendering::Resources {
    struct Provider;
}

namespace CE::Assets::Materials {
    /** Strongly retains immutable material recipes and their pipeline generations.
     * A builder owns the explicit backend mappings and bootstrap choices. Build
     * the complete candidate before publishing; failed reload leaves the old recipe.
     * Invoke synchronously on the active provider/loading owner, outside cache locks.
     * Builder captures must survive the call; its external side effects are not rolled back.
     */
    struct CE_TYPE_API Manager final : CE::Assets::Cache::Store<const CE::Rendering::Resources::Material>,
                                       CE::State::Singleton<Manager> {
        /** Synchronous candidate factory; captures remain live through invocation and external side effects are not rolled back. */
        using Builder = std::function<std::shared_ptr<const CE::Rendering::Resources::Material>(CE::Rendering::Resources::Provider&)>;

        Manager() = default;
        ~Manager() override = default;
        // Exact path key; an existing entry skips the builder.
        CE_API void load(const std::filesystem::path& key, CE::Rendering::Resources::Provider& provider, const Builder& builder);
        // Replace or insert one generation; empty builders/null candidates throw.
        CE_API void reload(const std::filesystem::path& key, CE::Rendering::Resources::Provider& provider, const Builder& builder);
    };
}

namespace CE::State {
    template <> CE_API auto Singleton<CE::Assets::Materials::Manager>::storage() -> Storage&;
}

/**
 * @fn CE::Assets::Materials::Manager::Manager()
 * @brief Create an empty material cache facade sharing the one provider/loading-owner domain.
 */

/**
 * @fn CE::Assets::Materials::Manager::~Manager()
 * @brief Drop this cache's strong handles; independent frame/read handles retain earlier generations.
 */

/**
 * @fn void CE::Assets::Materials::Manager::load(const std::filesystem::path& key, CE::Rendering::Resources::Provider& provider, const Builder& builder)
 * @brief Bind/verify the loading owner and build only when the exact key is absent.
 * @param key Exact path key, without implicit normalization or discovery.
 * @param provider Single active resource domain, used synchronously on its loading owner.
 * @param builder Candidate factory, not invoked for an existing key.
 * Missing keys use reload() validation/publication behavior; existing generations remain unchanged.
 * @see [Resource residency](cheryl-guide:resources/resource-residency)
 */

/**
 * @fn void CE::Assets::Materials::Manager::reload(const std::filesystem::path& key, CE::Rendering::Resources::Provider& provider, const Builder& builder)
 * @brief Build a complete immutable candidate before replacing/inserting one cache key.
 * @param key Exact cache key.
 * @param provider Active loading-owner backend passed to the builder.
 * @param builder Nonempty synchronous callable returning a nonnull validated material.
 * A builder exception, null candidate or publication allocation failure preserves the previous entry. External builder effects are not rolled back; old readers retain old generations. Empty builders throw invalid_args, null candidates failed_operation.
 */
