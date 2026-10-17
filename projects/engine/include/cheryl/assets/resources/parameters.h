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
    // Units are zero-based binding requests, never mutable state on a cached image.
    struct ImageBinding {
        std::shared_ptr<const CE::Rendering::Resources::Image> image;
        std::uint32_t unit = 0;
        // Absence selects the image's immutable default sampling.
        std::shared_ptr<const CE::Rendering::Sampling::Sampler> sampler = {};
    };

    // Values/keys are owned; copying a binding retains its image and sampler. CPU
    // validation/resolution requires stable inputs but no graphics context.
    using Value = std::variant<float, int, unsigned int, bool, glm::vec2, glm::vec3, glm::vec4, glm::mat4, ImageBinding>;
    using Set = std::map<std::string, Value, std::less<>>;

    enum class Type { Float, Int, UInt, Bool, Vec2, Vec3, Vec4, Mat4, Sampler2D };

    enum class Semantic { Custom, Projection, View, Model, Alpha, Scale };

    // Keys belong to the pipeline's public contract. Backend uniform names are separate.
    struct Definition {
        std::string key;
        Type type = Type::Float;
        bool required = true;
        Semantic semantic = Semantic::Custom;
        std::optional<Value> default_value;
    };

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
