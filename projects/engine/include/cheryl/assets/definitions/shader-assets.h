#pragma once

#include <cheryl/export.h>

#include <assets/definitions/pipeline.h>
#include <assets/resources/sampler.h>

#include <map>
#include <string>
#include <string_view>
#include <variant>

namespace CE::Assets::Shaders {
    /** Stage support of the portable shader-recipe format. */
    enum class Stage { Vertex, ///< Transforms vertices and emits interpolants.
        Fragment ///< Computes fragment color from interpolants/resources.
    };

    // Owned literals deliberately cannot retain images, samplers or backend handles.
    /** Serializable CPU parameter value; images, samplers and native handles are excluded. */
    using Literal = std::variant<float, int, unsigned int, bool, glm::vec2, glm::vec3, glm::vec4, glm::mat4>;

    /** Owned source snapshot; path identifies its origin, bytes supply the prepared contents. */
    struct Source {
        Stage stage = Stage::Vertex; ///< Declared vertex or fragment role; duplicate roles reject validation.
        std::filesystem::path path; ///< Nonempty source identity/path retained for backend construction and diagnostics.
        std::string bytes; ///< Owned source text; preparation avoids reopening the source at upload.
    };

    /** Owned vertex/fragment program recipe; validate_program() performs structural checks. */
    struct ProgramRecipe {
        std::string id; ///< Exact qualified namespace:name program identity.
        std::vector<Source> sources; ///< Exactly one vertex and one fragment source when validated.
    };

    /** Serializable public parameter contract; engine semantics forbid custom defaults. */
    struct ParameterRecipe {
        std::string key; ///< Unique nonempty public key, separate from backend uniform names.
        CE::Rendering::Parameters::Type type = CE::Rendering::Parameters::Type::Float; ///< Exact resolved value type.
        bool required = true; ///< Required in a complete resolved draw when true.
        CE::Rendering::Parameters::Semantic semantic = CE::Rendering::Parameters::Semantic::Custom; ///< Engine source or Custom layered value.
        std::optional<Literal> default_value = {}; ///< Optional finite typed fallback; absent for engine-semantic parameters.
    };

    /** Owned material/pipeline recipe; backend bindings are validated by the selected builder.
     * @see [Rendering guide](cheryl-guide:rendering/pipelines-and-materials)
     */
    struct MaterialRecipe {
        std::string id; ///< Exact qualified namespace:name material identity.
        std::string program; ///< Qualified identity of the matching ProgramRecipe.
        CE::Rendering::Geometry::Layout2D vertex_layout = CE::Rendering::Geometry::Layout2D::Position3UV2; ///< Required uploaded vertex layout.
        CE::Rendering::Geometry::Topology topology = CE::Rendering::Geometry::Topology::Triangles; ///< Required uploaded primitive topology.
        CE::Rendering::Pipelines::State2D state; ///< Fixed pipeline state, checked against pass constraints at submission.
        std::vector<ParameterRecipe> parameters; ///< Public keys, exact types and engine-semantic sources.
        std::map<std::string, Literal, std::less<>> defaults; ///< Finite material custom values, above pass values and below draw values.
        // FX policies address sampler parameters, never image assets.
        std::map<std::string, CE::Rendering::Sampling::Options, std::less<>> sampling; ///< Per-sampler-key policy, never an image asset selector.
        // Serialized backend payloads are interpreted only by that backend's builder.
        std::map<std::string, std::string, std::less<>> bindings; ///< Serialized mappings interpreted only by the selected backend builder.
    };

    /** Owned shader document prepared without native resources. */
    struct Manifest {
        std::filesystem::path source; ///< Manifest origin for diagnostics and source-relative parsing.
        std::string name_space; ///< Namespace used to qualify local recipe identities.
        std::vector<ProgramRecipe> programs; ///< Program recipes in document order.
        std::vector<MaterialRecipe> materials; ///< Material recipes in document order.
    };

    [[nodiscard]] CE_API bool is_qualified_id(std::string_view id);
    [[nodiscard]] CE_API CE::Rendering::Parameters::Value parameter_value(const Literal& literal);
    [[nodiscard]] CE_API CE::Rendering::Parameters::Set defaults(const MaterialRecipe& recipe);
    [[nodiscard]] CE_API CE::Rendering::Pipelines::Definition
    pipeline_definition(const MaterialRecipe& recipe, const ProgramRecipe& program);
    CE_API void validate_program(const ProgramRecipe& recipe);
    CE_API void validate_material(const MaterialRecipe& recipe, const ProgramRecipe& program);
}

/**
 * @fn bool CE::Assets::Shaders::is_qualified_id(std::string_view id)
 * @brief Check the lowercase namespace:name grammar without allocating or consulting a catalogue.
 * @param id Borrowed identity; namespace/name start with a lowercase letter or digit and permit underscore/hyphen, with dots permitted only in the namespace.
 * @return False for empty parts, absent/extra colons or other characters; this does not establish that an asset is loaded.
 */

/**
 * @fn CE::Rendering::Parameters::Value CE::Assets::Shaders::parameter_value(const Literal& literal)
 * @brief Copy a serializable literal into its matching parameter Value alternative.
 * @param literal Stable CPU literal; no coercion or finite-value validation occurs.
 * @return Owned value with no image/native handles.
 */

/**
 * @fn CE::Rendering::Parameters::Set CE::Assets::Shaders::defaults(const MaterialRecipe& recipe)
 * @brief Copy material literals to an owned public-key parameter set.
 * @param recipe Stable recipe borrowed for the call.
 * @return Copied custom values; this conversion alone does not validate keys/types or completeness.
 */

/**
 * @fn CE::Rendering::Pipelines::Definition CE::Assets::Shaders::pipeline_definition(const MaterialRecipe& recipe, const ProgramRecipe& program)
 * @brief Convert recipe state, source paths and public parameter contracts into a CPU pipeline definition.
 * @param recipe Material layout/topology/state/parameters.
 * @param program Source paths used by this pipeline.
 * @return Owned metadata snapshot; source bytes, material defaults, sampling and backend bindings stay in their separate recipes.
 * No validation, file I/O, native compilation or publication occurs.
 */

/**
 * @fn void CE::Assets::Shaders::validate_program(const ProgramRecipe& recipe)
 * @brief Validate qualified identity and exactly one nonempty-path vertex/fragment source each.
 * @param recipe Stable CPU program recipe.
 * @throws CE::Exceptions::invalid_args For invalid identities, empty paths, unsupported or repeated stages.
 * Does not compile source text, open paths or require nonempty source bytes.
 */

/**
 * @fn void CE::Assets::Shaders::validate_material(const MaterialRecipe& recipe, const ProgramRecipe& program)
 * @brief Validate portable recipe/program agreement, pipeline structure and finite typed literals.
 * @param recipe Qualified material referring exactly to program.id.
 * @param program Valid vertex/fragment recipe.
 * @throws CE::Exceptions::invalid_args For malformed identities/state/contracts/literals or sampling policies not naming sampler parameters.
 * Backend binding syntax, native reflection and resource construction remain builder responsibilities; inputs are unchanged.
 */
