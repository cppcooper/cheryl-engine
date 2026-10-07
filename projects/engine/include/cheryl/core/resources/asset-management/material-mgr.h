#pragma once

#include <assets/resources/pipeline.h>
#include <templates/asset-mgr.h>
#include <templates/singleton.h>

#include <functional>
#include <filesystem>
#include <memory>

namespace CE::Assets {
    struct ResourceProvider;

    /** Strongly retains immutable material recipes and their pipeline generations.
     * A builder owns the explicit backend mappings and bootstrap choices. Build
     * the complete candidate before publishing; failed reload leaves the old recipe.
     * Invoke synchronously on the active provider/loading owner, outside cache locks.
     * Builder captures must survive the call; its external side effects are not rolled back.
     */
    struct MaterialMgr final : AssetMgr<const Material>,
                               Singleton_CTS<MaterialMgr> {
        using Builder = std::function<std::shared_ptr<const Material>(ResourceProvider&)>;

        MaterialMgr() = default;
        ~MaterialMgr() override = default;
        // Exact path key; an existing entry skips the builder.
        void load_material(const std::filesystem::path& key, ResourceProvider& provider, const Builder& builder);
        // Replace or insert one generation; empty builders/null candidates throw.
        void reload_material(const std::filesystem::path& key, ResourceProvider& provider, const Builder& builder);
    };
}
