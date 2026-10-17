#pragma once

#include <cheryl/export.h>

#include <assets/definitions/shader-assets.h>
#include <assets/resources/pipeline.h>

namespace CE::Assets::Shaders {
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
    };
}
