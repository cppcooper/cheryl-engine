#pragma once

#include <assets/definitions/shader-assets.h>
#include <assets/resources/pipeline.h>
#include <templates/asset-mgr.h>
#include <templates/singleton.h>

namespace CE::Assets {
    struct ShaderProgramAsset {
        const ShaderProgramRecipe recipe;
        const std::shared_ptr<Shader> executable;
    };

    struct ShaderMaterialAsset {
        const ShaderMaterialRecipe recipe;
        const std::shared_ptr<const ShaderProgramAsset> program;
        const std::shared_ptr<const Material> material;
    };

    struct ShaderAsset {
        const std::variant<std::shared_ptr<const ShaderProgramAsset>, std::shared_ptr<const ShaderMaterialAsset>> value;
    };

    // Qualified identities share the existing provider domain. Metadata and the
    // resource it describes are published as one immutable generation.
    struct ShaderAssetMgr final : AssetMgr<const ShaderAsset, std::string>, Singleton_CTS<ShaderAssetMgr> {
        [[nodiscard]] std::shared_ptr<const ShaderProgramAsset> get_program(const std::string& id) const;
        [[nodiscard]] std::shared_ptr<const Material> get_material(const std::string& id) const;
        // Checks the complete batch before publication; unsupported capabilities
        // and existing identities of the wrong kind reject explicitly.
        void validate_assets(const std::vector<ShaderAssetManifest>& manifests, ResourceProvider& provider, bool replace = false) const;
        void load_assets(const std::vector<ShaderAssetManifest>& manifests, ResourceProvider& provider, bool replace = false);
    };
}
