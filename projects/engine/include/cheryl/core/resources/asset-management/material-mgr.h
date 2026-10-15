#pragma once

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
    struct Manager final : CE::Assets::Cache::Store<const CE::Rendering::Resources::Material>,
                           CE::State::Singleton<Manager> {
        using Builder = std::function<std::shared_ptr<const CE::Rendering::Resources::Material>(CE::Rendering::Resources::Provider&)>;

        Manager() = default;
        ~Manager() override = default;
        // Exact path key; an existing entry skips the builder.
        void load(const std::filesystem::path& key, CE::Rendering::Resources::Provider& provider, const Builder& builder);
        // Replace or insert one generation; empty builders/null candidates throw.
        void reload(const std::filesystem::path& key, CE::Rendering::Resources::Provider& provider, const Builder& builder);
    };
}
