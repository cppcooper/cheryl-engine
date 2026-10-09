#pragma once

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

namespace CE::Assets {
    // Units are zero-based binding requests, never mutable state on a cached image.
    struct ImageBinding {
        std::shared_ptr<const Image> image;
        std::uint32_t unit = 0;
        // Absence selects the image's immutable default sampling.
        std::shared_ptr<const Sampler> sampler = {};
    };

    // Values/keys are owned; copying a binding retains its image and sampler. CPU
    // validation/resolution requires stable inputs but no graphics context.
    using ParameterValue = std::variant<float, int, unsigned int, bool, glm::vec2, glm::vec3, glm::vec4, glm::mat4, ImageBinding>;
    using ParameterSet = std::map<std::string, ParameterValue, std::less<>>;

    enum class ParameterType { Float, Int, UInt, Bool, Vec2, Vec3, Vec4, Mat4, Sampler2D };

    enum class ParameterSemantic { Custom, Projection, View, Model, Alpha, Scale };

    // Keys belong to the pipeline's public contract. Backend uniform names are separate.
    struct ParameterDefinition {
        std::string key;
        ParameterType type = ParameterType::Float;
        bool required = true;
        ParameterSemantic semantic = ParameterSemantic::Custom;
        std::optional<ParameterValue> default_value;
    };

    using ParameterContract = std::vector<ParameterDefinition>;

    [[nodiscard]] ParameterType parameter_type(const ParameterValue& value);
    // Throws invalid_args for malformed keys/types/semantics/defaults. Does not
    // reflect a program or validate image domains, unit limits or numeric ranges.
    void validate_parameter_contract(const ParameterContract& contract);
    // Validate one partial custom layer: rejects unknown/semantic keys, wrong types
    // and null images; does not require complete values or distinct sampler units.
    void validate_parameter_values(const ParameterContract& contract, const ParameterSet& values);
    // Validate a complete packet, including already resolved engine values. Requires
    // all required keys and distinct sampler units; does not verify semantic provenance.
    void validate_resolved_parameters(const ParameterContract& contract, const ParameterSet& values);

    // Copies defaults < pass < material < draw. Engine semantics have a single owner
    // and cannot be overridden by a custom-value layer. Validate every input layer,
    // even overridden entries, then the complete result. Optional missing keys are
    // absent. Failure leaves inputs unchanged and returns no partially resolved set.
    [[nodiscard]] ParameterSet resolve_parameters(
        const ParameterContract& contract,
        const ShaderPass& pass_semantics,
        const ShaderDraw& draw_semantics,
        const ParameterSet& pass_values,
        const ParameterSet& material_values,
        const ParameterSet& draw_values
    );
}
