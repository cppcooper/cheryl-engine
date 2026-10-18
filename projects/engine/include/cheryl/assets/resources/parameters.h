#pragma once

#include <cheryl/export.h>

#include "image.h"
#include "sampler.h"
#include "shader.h"

#include <glm.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace CE::Rendering::Parameters {
    /** One retained image/sampler request; binding does not mutate cached image policy. */
    // Units are zero-based binding requests, never mutable state on a cached image.
    struct ImageBinding {
        std::shared_ptr<const CE::Rendering::Resources::Image> image; ///< Retained image; validated parameter values require a nonnull handle.
        std::uint32_t unit = 0; ///< Zero-based unit; complete resolved packets require distinct units.
        // Absence selects the image's immutable default sampling.
        std::shared_ptr<const CE::Rendering::Sampling::Sampler> sampler = {}; ///< Retained override, or empty to use image/default material sampling.
    };

    // Values/keys are owned; copying a binding retains its image and sampler. CPU
    // validation/resolution requires stable inputs but no graphics context.
    /** Owned scalar/vector/matrix value or retained image binding. */
    using Value = std::variant<float, int, unsigned int, bool, glm::vec2, glm::vec3, glm::vec4, glm::mat4, ImageBinding>;
    /** Owned public keys and values, ordered lexically with transparent string lookup. */
    using Set = std::map<std::string, Value, std::less<>>;

    /** Contract type of a parameter; Sampler2D requires an ImageBinding. */
    enum class Type {
        Float, ///< One floating scalar.
        Int, ///< One signed integer scalar.
        UInt, ///< One unsigned integer scalar.
        Bool, ///< One boolean value.
        Vec2, ///< Two floating components.
        Vec3, ///< Three floating components.
        Vec4, ///< Four floating components.
        Mat4, ///< One GLM 4 by 4 floating matrix.
        Sampler2D ///< Retained ImageBinding with a zero-based unit and optional sampler.
    };

    /** Value owner: Custom uses explicit layers; other values come only from engine semantics. */
    enum class Semantic {
        Custom, ///< Resolve explicit contract/pass/material/draw layers.
        Projection, ///< Copy PassSemantics::projection; requires Mat4.
        View, ///< Copy PassSemantics::view; requires Mat4.
        Model, ///< Copy DrawSemantics::model; requires Mat4.
        Alpha, ///< Copy DrawSemantics::alpha without clamping; requires Float.
        Scale ///< Copy DrawSemantics::scale without unit conversion; requires Float.
    };

    /** Public parameter definition; backend uniform names are separate mappings.
     * Engine semantics must have the matching type, no default, and a unique semantic role.
     */
    struct Definition {
        std::string key; ///< Nonempty unique public key, unrelated to backend uniform spelling.
        Type type = Type::Float; ///< Required Value alternative; values are not coerced.
        bool required = true; ///< Complete packets must contain this key when true.
        Semantic semantic = Semantic::Custom; ///< Engine source, or Custom for default/pass/material/draw layers.
        std::optional<Value> default_value; ///< Optional typed fallback for a custom key; may retain image resources.
    };

    /** Ordered definitions; validation rejects duplicate or empty public keys. */
    using Contract = std::vector<Definition>;

    [[nodiscard]] CE_API Type type_of(const Value& value);
    // Throws invalid_args for malformed keys/types/semantics/defaults. Does not
    // reflect a program or validate image domains, unit limits or numeric ranges.
    CE_API void validate_contract(const Contract& contract);
    // Validate one partial custom layer: rejects unknown/semantic keys, wrong types
    // and null images; does not require complete values or distinct sampler units.
    CE_API void validate_values(const Contract& contract, const Set& values);
    // Validate a complete packet, including already resolved engine values. Requires
    // all required keys and distinct sampler units; does not verify semantic provenance.
    CE_API void validate_resolved(const Contract& contract, const Set& values);

    // Copies defaults < pass < material < draw. Engine semantics have a single owner
    // and cannot be overridden by a custom-value layer. Validate every input layer,
    // even overridden entries, then the complete result. Optional missing keys are
    // absent. Failure leaves inputs unchanged and returns no partially resolved set.
    [[nodiscard]] CE_API Set resolve(
        const Contract& contract,
        const PassSemantics& pass_semantics,
        const DrawSemantics& draw_semantics,
        const Set& pass_values,
        const Set& material_values,
        const Set& draw_values
    );
}

/**
 * @fn CE::Rendering::Parameters::Type CE::Rendering::Parameters::type_of(const Value& value)
 * @brief Inspect the active Value alternative without converting or binding it.
 * @param value Stable value borrowed for the call.
 * @return Its exact parameter type, including Sampler2D for ImageBinding.
 */

/**
 * @fn void CE::Rendering::Parameters::validate_contract(const Contract& contract)
 * @brief Validate unique keys, known types, semantic/type agreement and typed defaults.
 * @param contract Stable CPU definitions; no resource is bound or reflected.
 * @throws CE::Exceptions::invalid_args For malformed definitions or null default images.
 * Does not validate numeric ranges, backend domains, unit limits or source/program reflection. Inputs remain unchanged.
 */

/**
 * @fn void CE::Rendering::Parameters::validate_values(const Contract& contract, const Set& values)
 * @brief Validate a partial custom-value layer, including the contract itself.
 * @param contract Definitions for permitted keys and exact types.
 * @param values Owned inputs borrowed for validation; semantic overrides are forbidden.
 * @throws CE::Exceptions::invalid_args For an unknown/semantic key, wrong type or null image.
 * Missing required keys and duplicate image units across values are allowed at this stage; no backend calls occur.
 */

/**
 * @fn void CE::Rendering::Parameters::validate_resolved(const Contract& contract, const Set& values)
 * @brief Validate a complete packet, including resolved engine-semantic values.
 * @param contract Stable definitions, also validated.
 * @param values Complete values with all required keys and distinct image units.
 * @throws CE::Exceptions::invalid_args For unknown/missing keys, wrong types, null images or duplicate units.
 * Does not prove where semantic values originated or verify native domains/unit limits. Inputs are unchanged.
 */

/**
 * @fn CE::Rendering::Parameters::Set CE::Rendering::Parameters::resolve(const Contract& contract, const PassSemantics& pass_semantics, const DrawSemantics& draw_semantics, const Set& pass_values, const Set& material_values, const Set& draw_values)
 * @brief Resolve an owned CPU value set with defaults < pass < material < draw precedence.
 * @param contract Public key/type/semantic definitions.
 * @param pass_semantics Projection/view source; copied into engine-semantic keys.
 * @param draw_semantics Model/alpha/scale source; no shared clamping is added.
 * @param pass_values Lowest-priority explicit custom layer.
 * @param material_values Per-material custom layer.
 * @param draw_values Highest-priority explicit custom layer.
 * @return Complete independent set retaining selected image/sampler handles; optional absent keys stay absent.
 * Every input layer validates even if overridden; custom layers cannot set engine semantics. Validation/allocation failures return no partial set and leave inputs unchanged.
 * @see [Rendering guide](cheryl-guide:rendering/pipelines-and-materials)
 */
