#pragma once

#include "image.h"
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
    };

    using ParameterValue = std::variant<float, int, unsigned int, bool, glm::vec2, glm::vec3, glm::vec4, glm::mat4, ImageBinding>;
    using ParameterSet = std::map<std::string, ParameterValue, std::less<>>;

    enum class ParameterType {
        Float,
        Int,
        UInt,
        Bool,
        Vec2,
        Vec3,
        Vec4,
        Mat4,
        Sampler2D
    };

    enum class ParameterSemantic {
        Custom,
        Projection,
        View,
        Model,
        Alpha,
        Scale
    };

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
    void validate_parameter_contract(const ParameterContract& contract);
    void validate_parameter_values(
        const ParameterContract& contract,
        const ParameterSet& values
    );
    // Validate a complete packet, including engine values already resolved by its producer.
    void validate_resolved_parameters(
        const ParameterContract& contract,
        const ParameterSet& values
    );

    // Copies defaults < pass < material < draw. Engine semantics have a single owner
    // and cannot be overridden by a custom-value layer. Optional missing keys are absent.
    [[nodiscard]] ParameterSet resolve_parameters(
        const ParameterContract& contract,
        const ShaderPass& pass_semantics,
        const ShaderDraw& draw_semantics,
        const ParameterSet& pass_values,
        const ParameterSet& material_values,
        const ParameterSet& draw_values
    );
}
