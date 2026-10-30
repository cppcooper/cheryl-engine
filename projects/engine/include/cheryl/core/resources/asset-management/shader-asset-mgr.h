#pragma once

#include <cheryl/export.h>

#include <assets/definitions/shader-assets.h>
#include <assets/resources/pipeline.h>
#include <templates/asset-mgr.h>
#include <templates/singleton.h>

namespace CE::Assets::Shaders {
    /** Immutable source recipe paired with its linked executable generation. */
    struct Program {
        const ProgramRecipe recipe; ///< Owned recipe snapshot, including prepared source bytes.
        const std::shared_ptr<CE::Rendering::Resources::Program> executable; ///< Retained backend program; mutable use still obeys backend owner affinity.
    };

    /** Immutable material metadata retaining the exact source-program generation it describes. */
    struct Material {
        const MaterialRecipe recipe; ///< Owned material recipe snapshot.
        const std::shared_ptr<const Program> program; ///< Exact retained program recipe/executable generation.
        const std::shared_ptr<const CE::Rendering::Resources::Material> material; ///< Complete immutable resolved backend material.
    };

    /** Tagged immutable catalogue generation; an identity has exactly one kind. */
    struct Asset {
        const std::variant<std::shared_ptr<const Program>, std::shared_ptr<const Material>> value; ///< Retained program or material metadata/native pair.
    };

    /** Qualified identities share the existing provider domain. Metadata and the
     * resource it describes are published as one immutable generation.
     */
    struct CE_TYPE_API Manager final : CE::Assets::Cache::Store<const Asset, std::string>,
                                       CE::State::Singleton<Manager> {
        [[nodiscard]] CE_API std::shared_ptr<const Program> find_program(const std::string& id) const;
        [[nodiscard]] CE_API std::shared_ptr<const CE::Rendering::Resources::Material> find_material(const std::string& id) const;
        // Checks the complete batch before publication; unsupported capabilities
        // and existing identities of the wrong kind reject explicitly.
        CE_API void
        validate(const std::vector<Manifest>& manifests, CE::Rendering::Resources::Provider& provider, bool replace = false) const;
        CE_API void load(const std::vector<Manifest>& manifests, CE::Rendering::Resources::Provider& provider, bool replace = false);
    };
}

namespace CE::State {
    template <> CE_API auto Singleton<CE::Assets::Shaders::Manager>::storage() -> Storage&;
}

/**
 * @fn std::shared_ptr<const CE::Assets::Shaders::Program> CE::Assets::Shaders::Manager::find_program(const std::string& id) const
 * @brief Look up an exact qualified program identity without compiling/uploading.
 * @param id Exact namespace:name key, copied only for lookup as needed.
 * @return Retained immutable recipe/executable pair, or null when missing.
 * A material identity throws invalid_args. Shared cache locking protects lookup; retained handles survive replacement/clearing but retain the original native-domain limits.
 */

/**
 * @fn std::shared_ptr<const CE::Rendering::Resources::Material> CE::Assets::Shaders::Manager::find_material(const std::string& id) const
 * @brief Look up an exact qualified material identity without native work.
 * @param id Exact namespace:name key.
 * @return Retained immutable material, or null when missing.
 * A program identity throws invalid_args. Shared cache locking protects lookup; retained handles survive replacement/clearing but native use needs the original live domain.
 */

/**
 * @fn void CE::Assets::Shaders::Manager::validate(const std::vector<Manifest>& manifests, CE::Rendering::Resources::Provider& provider, bool replace) const
 * @brief Validate the complete selected recipe batch and backend CPU capabilities before publication.
 * @param manifests Stable selected program/material recipes; material program references must be selected in this batch.
 * @param provider Compatible active loading-owner domain; a nonempty batch requires its optional Builder.
 * @param replace False reuses existing same-kind generations; true validates replacement candidates.
 * Duplicate/wrong-kind IDs, unselected programs, invalid recipes or unsupported capabilities reject without native construction/cache publication. An empty batch is allowed; existing provider-owner restrictions still apply.
 */

/**
 * @fn void CE::Assets::Shaders::Manager::load(const std::vector<Manifest>& manifests, CE::Rendering::Resources::Provider& provider, bool replace)
 * @brief Validate then build/publish programs before materials on the active provider owner/current context.
 * @param manifests Stable selected complete recipe batch.
 * @param provider Single supported provider domain with shader builder capability.
 * @param replace False preserves existing same-kind keys; true replaces each successful candidate independently.
 * Metadata/executable pairs publish as immutable generations. Native/allocation/publication failure can leave earlier new entries/replacements visible; no batch rollback occurs. Existing retained readers keep old generations.
 * @see [Publication and retry](cheryl-guide:assets/asset-loading#publication-and-retry)
 */
