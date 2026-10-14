#pragma once

#include <assets/definitions/shader-assets.h>
#include <assets/resources/pipeline.h>
#include <templates/asset-mgr.h>
#include <templates/singleton.h>

namespace CE::Assets::Shaders {
    struct Program {
        const ProgramRecipe recipe;
        const std::shared_ptr<CE::Rendering::Resources::Program> executable;
    };

    struct Material {
        const MaterialRecipe recipe;
        const std::shared_ptr<const Program> program;
        const std::shared_ptr<const CE::Rendering::Resources::Material> material;
    };

    struct Asset {
        const std::variant<std::shared_ptr<const Program>, std::shared_ptr<const Material>> value;
    };

    /** Qualified identities share the existing provider domain. Metadata and the
     * resource it describes are published as one immutable generation.
     */
    struct Manager final : CE::Assets::Cache::Store<const Asset, std::string>,
                           CE::State::Singleton<Manager> {
        [[nodiscard]] std::shared_ptr<const Program> find_program(const std::string& id) const;
        /** Look up an already loaded material without compiling or uploading resources.
         * The retained handle survives cache replacement/clearing; native use
         * still obeys its provider lifetime. Lookup uses the cache lock and rejects
         * an identity that belongs to a program rather than a material.
         * @param id Exact qualified namespace:name identity.
         * @return Retained immutable material, or null for a missing identity.
         */
        [[nodiscard]] std::shared_ptr<const CE::Rendering::Resources::Material> find_material(const std::string& id) const;
        // Checks the complete batch before publication; unsupported capabilities
        // and existing identities of the wrong kind reject explicitly.
        void validate(const std::vector<Manifest>& manifests, CE::Rendering::Resources::Provider& provider, bool replace = false) const;
        void load(const std::vector<Manifest>& manifests, CE::Rendering::Resources::Provider& provider, bool replace = false);
    };
}
