#pragma once

#include <assets/definitions/shader-assets.h>
#include <assets/resources/pipeline.h>

namespace CE::Assets {
    // Optional provider-owned capability. Validation is CPU-only; construction
    // runs on that provider's loading owner and returns complete native candidates.
    struct ShaderAssetBuilder {
        virtual ~ShaderAssetBuilder() = default;
        virtual void validate_program(const ShaderProgramRecipe& recipe) const = 0;
        virtual void validate_material(const ShaderMaterialRecipe& recipe, const ShaderProgramRecipe& program) const = 0;
        [[nodiscard]] virtual std::shared_ptr<Shader> build_program(const ShaderProgramRecipe& recipe) = 0;
        [[nodiscard]] virtual std::shared_ptr<const Material> build_material(
            const ShaderMaterialRecipe& recipe, const ShaderProgramRecipe& program, const std::shared_ptr<Shader>& executable
        ) = 0;
    };
}
