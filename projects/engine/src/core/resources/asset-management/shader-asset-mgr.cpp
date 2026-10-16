#include <core/resources/asset-management/shader-asset-mgr.h>

#include <assets/resources/resource-provider.h>
#include <assets/resources/shader-asset-builder.h>
#include <internals/exceptions.h>

#include <unordered_map>
#include <unordered_set>

namespace CE::Assets::Shaders {
    std::shared_ptr<const Program> Manager::find_program(const std::string& id) const {
        const auto entry = get_asset(id);
        if (!entry)
            return nullptr;
        if (const auto* program = std::get_if<std::shared_ptr<const Program>>(&entry->value))
            return *program;
        throw Exceptions::invalid_args(CE_HERE, "Shader asset '" + id + "' is a material, not a program recipe");
    }

    std::shared_ptr<const CE::Rendering::Resources::Material> Manager::find_material(const std::string& id) const {
        const auto entry = get_asset(id);
        if (!entry)
            return nullptr;
        if (const auto* material = std::get_if<std::shared_ptr<const Material>>(&entry->value))
            return (*material)->material;
        throw Exceptions::invalid_args(CE_HERE, "Shader asset '" + id + "' is a program recipe, not a material");
    }

    void Manager::validate(const std::vector<Manifest>& manifests, CE::Rendering::Resources::Provider& provider, const bool replace) const {
        verify_provider(provider);
        if (manifests.empty())
            return;
        const auto* builder = provider.shader_asset_builder();
        if (!builder)
            throw Exceptions::failed_operation(CE_HERE, "Resource provider does not support shader asset recipes");
        std::unordered_map<std::string, const ProgramRecipe*> programs;
        std::vector<std::shared_ptr<const Program>> retained;
        std::unordered_set<std::string> identities;
        for (const auto& manifest : manifests)
            for (const auto& recipe : manifest.programs) {
                validate_program(recipe);
                if (!identities.emplace(recipe.id).second)
                    throw Exceptions::invalid_args(CE_HERE, "Duplicate shader asset '" + recipe.id + "'");
                const auto existing = find_program(recipe.id);
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
                validate_material(recipe, *program->second);
                const auto existing = find_material(recipe.id);
                if (replace || !existing)
                    builder->validate_material(recipe, *program->second);
            }
    }

    void Manager::load(const std::vector<Manifest>& manifests, CE::Rendering::Resources::Provider& provider, const bool replace) {
        validate(manifests, provider, replace);
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
                auto program = std::make_shared<const Program>(Program{recipe, std::move(executable)});
                auto candidate = std::make_shared<const Asset>(Asset{std::move(program)});
                if (replace)
                    replace_asset(recipe.id, std::move(candidate));
                else
                    publish_asset(recipe.id, std::move(candidate));
            }
        for (const auto& manifest : manifests)
            for (const auto& recipe : manifest.materials) {
                if (!replace && contains(recipe.id))
                    continue;
                const auto program = find_program(recipe.program);
                if (!program)
                    throw Exceptions::failed_operation(CE_HERE, "Shader material's prepared program was not published");
                auto material = builder.build_material(recipe, program->recipe, program->executable);
                if (!material)
                    throw Exceptions::failed_operation(CE_HERE, "Shader builder returned an empty material '" + recipe.id + "'");
                auto asset = std::make_shared<const Material>(Material{recipe, program, std::move(material)});
                auto candidate = std::make_shared<const Asset>(Asset{std::move(asset)});
                if (replace)
                    replace_asset(recipe.id, std::move(candidate));
                else
                    publish_asset(recipe.id, std::move(candidate));
            }
    }
}

namespace CE::State {
    template <> auto Singleton<CE::Assets::Shaders::Manager>::storage() -> Storage& {
        static Storage value;
        return value;
    }
}
