#include <core/resources/asset-management/shader-asset-mgr.h>

#include <assets/resources/resource-provider.h>
#include <assets/resources/shader-asset-builder.h>
#include <internals/exceptions.h>

#include <unordered_map>
#include <unordered_set>

namespace CE::Assets {
    std::shared_ptr<const ShaderProgramAsset> ShaderAssetMgr::get_program(const std::string& id) const {
        const auto entry = get_asset(id);
        if (!entry)
            return nullptr;
        if (const auto* program = std::get_if<std::shared_ptr<const ShaderProgramAsset>>(&entry->value))
            return *program;
        throw Exceptions::invalid_args(CE_HERE, "Shader asset '" + id + "' is a material, not a program recipe");
    }

    std::shared_ptr<const Material> ShaderAssetMgr::get_material(const std::string& id) const {
        const auto entry = get_asset(id);
        if (!entry)
            return nullptr;
        if (const auto* material = std::get_if<std::shared_ptr<const ShaderMaterialAsset>>(&entry->value))
            return (*material)->material;
        throw Exceptions::invalid_args(CE_HERE, "Shader asset '" + id + "' is a program recipe, not a material");
    }

    void ShaderAssetMgr::validate_assets(const std::vector<ShaderAssetManifest>& manifests, ResourceProvider& provider, const bool replace) const {
        verify_provider(provider);
        if (manifests.empty())
            return;
        const auto* builder = provider.shader_asset_builder();
        if (!builder)
            throw Exceptions::failed_operation(CE_HERE, "Resource provider does not support shader asset recipes");
        std::unordered_map<std::string, const ShaderProgramRecipe*> programs;
        std::vector<std::shared_ptr<const ShaderProgramAsset>> retained;
        std::unordered_set<std::string> identities;
        for (const auto& manifest : manifests)
            for (const auto& recipe : manifest.programs) {
                validate_shader_program(recipe);
                if (!identities.emplace(recipe.id).second)
                    throw Exceptions::invalid_args(CE_HERE, "Duplicate shader asset '" + recipe.id + "'");
                const auto existing = get_program(recipe.id);
                programs.emplace(recipe.id, !replace && existing ? &existing->recipe : &recipe);
                if (!replace && existing)
                    retained.push_back(existing);
                if (replace || !existing)
                    builder->validate_program(recipe);
            }
        for (const auto& manifest : manifests)
            for (const auto& recipe : manifest.materials) {
                if (!identities.emplace(recipe.id).second)
                    throw Exceptions::invalid_args(CE_HERE, "Duplicate shader asset '" + recipe.id + "'");
                const auto program = programs.find(recipe.program);
                if (program == programs.end())
                    throw Exceptions::invalid_args(CE_HERE, "Shader material '" + recipe.id + "' references an unselected program '" + recipe.program + "'");
                validate_shader_material(recipe, *program->second);
                const auto existing = get_material(recipe.id);
                if (replace || !existing)
                    builder->validate_material(recipe, *program->second);
            }
    }

    void ShaderAssetMgr::load_assets(const std::vector<ShaderAssetManifest>& manifests, ResourceProvider& provider, const bool replace) {
        validate_assets(manifests, provider, replace);
        if (manifests.empty())
            return;
        bind_provider(provider);
        auto& builder = *provider.shader_asset_builder();
        for (const auto& manifest : manifests)
            for (const auto& recipe : manifest.programs) {
                if (!replace && contains(recipe.id))
                    continue;
                auto executable = builder.build_program(recipe);
                if (!executable)
                    throw Exceptions::failed_operation(CE_HERE, "Shader builder returned an empty program '" + recipe.id + "'");
                auto program = std::make_shared<const ShaderProgramAsset>(ShaderProgramAsset{recipe, std::move(executable)});
                auto candidate = std::make_shared<const ShaderAsset>(ShaderAsset{std::move(program)});
                if (replace)
                    replace_asset(recipe.id, std::move(candidate));
                else
                    publish_asset(recipe.id, std::move(candidate));
            }
        for (const auto& manifest : manifests)
            for (const auto& recipe : manifest.materials) {
                if (!replace && contains(recipe.id))
                    continue;
                const auto program = get_program(recipe.program);
                if (!program)
                    throw Exceptions::failed_operation(CE_HERE, "Shader material's prepared program was not published");
                auto material = builder.build_material(recipe, program->recipe, program->executable);
                if (!material)
                    throw Exceptions::failed_operation(CE_HERE, "Shader builder returned an empty material '" + recipe.id + "'");
                auto asset = std::make_shared<const ShaderMaterialAsset>(ShaderMaterialAsset{recipe, program, std::move(material)});
                auto candidate = std::make_shared<const ShaderAsset>(ShaderAsset{std::move(asset)});
                if (replace)
                    replace_asset(recipe.id, std::move(candidate));
                else
                    publish_asset(recipe.id, std::move(candidate));
            }
    }
}
