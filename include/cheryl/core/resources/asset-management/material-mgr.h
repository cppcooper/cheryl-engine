#pragma once

#include <assets/resources/pipeline.h>
#include <templates/asset-mgr.h>
#include <templates/singleton.h>

#include <functional>

namespace CE::Assets {
    struct ResourceProvider;

    /** Strongly retains immutable material recipes and their pipeline generations.
     * A builder owns the explicit backend mappings and bootstrap choices. Build
     * the complete candidate before publishing; failed reload leaves the old recipe.
     */
    struct MaterialMgr final : AssetMgr<const Material>, Singleton_CTS<MaterialMgr> {
        using Builder = std::function<std::shared_ptr<const Material>(ResourceProvider&)>;

        MaterialMgr() = default;
        ~MaterialMgr() override = default;
        void load_material(
            const std::filesystem::path& key,
            ResourceProvider& provider,
            const Builder& builder
        );
        void reload_material(
            const std::filesystem::path& key,
            ResourceProvider& provider,
            const Builder& builder
        );
    };
}
