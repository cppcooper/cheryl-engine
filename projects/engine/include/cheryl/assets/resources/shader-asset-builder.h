#pragma once

#include <cheryl/export.h>

#include <assets/definitions/shader-assets.h>
#include <assets/resources/pipeline.h>

namespace CE::Rendering::Resources {
    struct Provider;
}

namespace CE::Assets::Shaders {
    /** Request an embedded backend profile without application shader files or catalogue publication. */
    // Requests a backend-owned preset on the provider owner. No catalogue entry or
    // application shader files are required; unsupported/missing defaults reject.
    [[nodiscard]] CE_API std::shared_ptr<const CE::Rendering::Resources::Material> build_builtin_material(
        CE::Rendering::Resources::Provider& provider,
        CE::Rendering::Materials::Builtin2D kind,
        CE::Rendering::Pipelines::Blend blend = CE::Rendering::Pipelines::Blend::StraightAlpha
    );
    /** Optional provider-owned shader/material capability; borrow it only while its provider lives.
     * Validation is CPU-only; construction obeys provider upload affinity.
     * @see [Rendering guide](cheryl-guide:rendering/pipelines-and-materials)
     */
    // Optional provider-owned capability. Validation is CPU-only; construction
    // runs on that provider's loading owner and returns complete native candidates.
    struct CE_API Builder {
        virtual ~Builder() = default;
        virtual void validate_program(const ProgramRecipe& recipe) const = 0;
        virtual void validate_material(const MaterialRecipe& recipe, const ProgramRecipe& program) const = 0;
        [[nodiscard]] virtual std::shared_ptr<CE::Rendering::Resources::Program> build_program(const ProgramRecipe& recipe) = 0;
        [[nodiscard]] virtual std::shared_ptr<const CE::Rendering::Resources::Material> build_material(
            const MaterialRecipe& recipe,
            const ProgramRecipe& program,
            const std::shared_ptr<CE::Rendering::Resources::Program>& executable
        ) = 0;
        // Uses backend-owned shader sources and bindings, without application
        // files or catalogue publication. Construction obeys provider affinity;
        // unsupported implementations reject rather than choosing another backend.
        [[nodiscard]] virtual std::shared_ptr<const CE::Rendering::Resources::Material>
        build_builtin_material(CE::Rendering::Materials::Builtin2D kind, CE::Rendering::Pipelines::Blend blend);
    };
}

/**
 * @fn std::shared_ptr<const CE::Rendering::Resources::Material> CE::Assets::Shaders::build_builtin_material(CE::Rendering::Resources::Provider& provider, CE::Rendering::Materials::Builtin2D kind, CE::Rendering::Pipelines::Blend blend)
 * @brief Request a complete backend-owned default material on the provider loading owner.
 * @param provider Live provider with a shader-asset builder.
 * @param kind Geometry/semantic profile; ordinary assets use Textured or TexturedStrip.
 * @param blend Fixed alpha composition, StraightAlpha by default.
 * @return Retained immutable material; retain it in assets/frames for use.
 * A missing builder, unsupported profile or empty backend result rejects explicitly; native and allocation errors propagate. No application shader files or catalogue ID are needed.
 * @see [Rendering guide](cheryl-guide:rendering/pipelines-and-materials)
 */

/**
 * @fn CE::Assets::Shaders::Builder::~Builder()
 * @brief Release builder adapter state; the provider owns its availability/lifetime.
 */

/**
 * @fn void CE::Assets::Shaders::Builder::validate_program(const ProgramRecipe& recipe) const
 * @brief Validate a program recipe against this backend's supported stage rules without native construction.
 * @param recipe Stable CPU stage/source metadata.
 * Validation errors throw before publication; successful validation is not proof that native compilation will succeed.
 */

/**
 * @fn void CE::Assets::Shaders::Builder::validate_material(const MaterialRecipe& recipe, const ProgramRecipe& program) const
 * @brief Validate CPU material/program agreement and backend mapping syntax.
 * @param recipe Stable public parameter/state/binding recipe.
 * @param program Referenced program recipe.
 * No native compilation/reflection or cache publication occurs; unsupported capabilities reject explicitly.
 */

/**
 * @fn std::shared_ptr<CE::Rendering::Resources::Program> CE::Assets::Shaders::Builder::build_program(const ProgramRecipe& recipe)
 * @brief Build a complete executable candidate on the provider loading owner/current context.
 * @param recipe Owned-source recipe borrowed through this call.
 * @return Retained executable ready for material construction.
 * Validation/compile/link/allocation failures propagate; building alone publishes no catalogue entry.
 */

/**
 * @fn std::shared_ptr<const CE::Rendering::Resources::Material> CE::Assets::Shaders::Builder::build_material(const MaterialRecipe& recipe, const ProgramRecipe& program, const std::shared_ptr<CE::Rendering::Resources::Program>& executable)
 * @brief Build a complete immutable material candidate with native reflection/validation.
 * @param recipe Material public contract and backend mappings.
 * @param program Matching source recipe for executable.
 * @param executable Retained linked program from this provider domain.
 * @return Complete retained material, without catalogue publication.
 * Runs on the provider loading owner/current context. Invalid mappings/domains, unsupported state or backend/allocation errors reject the candidate; old published generations are unaffected.
 */

/**
 * @fn std::shared_ptr<const CE::Rendering::Resources::Material> CE::Assets::Shaders::Builder::build_builtin_material(CE::Rendering::Materials::Builtin2D kind, CE::Rendering::Pipelines::Blend blend)
 * @brief Optional backend implementation of an embedded default profile.
 * @param kind Requested vertex/semantic profile.
 * @param blend Fixed blend mode.
 * @return Complete retained material constructed on the provider owner.
 * The base throws failed_operation; adapters must not silently select another backend or require application shader files.
 */
