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
