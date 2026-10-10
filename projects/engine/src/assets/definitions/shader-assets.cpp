#include <assets/definitions/shader-assets.h>

#include <internals/exceptions.h>

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <utility>

namespace CE::Assets {
    bool is_shader_asset_id(const std::string_view id) {
        const auto colon = id.find(':');
        if (colon == std::string_view::npos || colon == 0 || colon + 1 == id.size())
            return false;
        const auto identifier = [](const std::string_view text, const bool dots) {
            const auto alnum = [](const char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); };
            return alnum(text.front()) && std::ranges::all_of(text, [&](const char c) {
                return alnum(c) || c == '_' || c == '-' || (dots && c == '.');
            });
        };
        return identifier(id.substr(0, colon), true) && identifier(id.substr(colon + 1), false);
    }

    ParameterValue shader_parameter_value(const ShaderLiteral& literal) {
        return std::visit([](const auto& value) -> ParameterValue { return value; }, literal);
    }

    ParameterSet shader_material_defaults(const ShaderMaterialRecipe& recipe) {
        ParameterSet result;
        for (const auto& [key, value] : recipe.defaults)
            result.emplace(key, shader_parameter_value(value));
        return result;
    }

    PipelineDefinition shader_pipeline_definition(const ShaderMaterialRecipe& recipe, const ShaderProgramRecipe& program) {
        PipelineDefinition result;
        for (const auto& source : program.sources)
            result.program_sources.push_back(source.path);
        result.vertex_layout = recipe.vertex_layout;
        result.topology = recipe.topology;
        result.state = recipe.state;
        for (const auto& parameter : recipe.parameters) {
            ParameterDefinition definition{parameter.key, parameter.type, parameter.required, parameter.semantic, {}};
            if (parameter.default_value)
                definition.default_value = shader_parameter_value(*parameter.default_value);
            result.parameters.push_back(std::move(definition));
        }
        return result;
    }

    void validate_shader_program(const ShaderProgramRecipe& recipe) {
        if (!is_shader_asset_id(recipe.id) || recipe.sources.size() != 2)
            throw Exceptions::invalid_args(CE_HERE, "Shader program requires a qualified ID and vertex/fragment sources");
        bool vertex = false;
        bool fragment = false;
        for (const auto& source : recipe.sources) {
            if (source.path.empty())
                throw Exceptions::invalid_args(CE_HERE, "Shader source path must not be empty");
            if (source.stage == ShaderStage::Vertex && !vertex)
                vertex = true;
            else if (source.stage == ShaderStage::Fragment && !fragment)
                fragment = true;
            else
                throw Exceptions::invalid_args(CE_HERE, "Shader program has an unsupported or repeated stage");
        }
    }

    void validate_shader_material(const ShaderMaterialRecipe& recipe, const ShaderProgramRecipe& program) {
        validate_shader_program(program);
        if (!is_shader_asset_id(recipe.id) || recipe.program != program.id)
            throw Exceptions::invalid_args(CE_HERE, "Shader material requires a qualified ID and its selected program");
        const auto pipeline = shader_pipeline_definition(recipe, program);
        validate_pipeline_definition(pipeline);
        validate_parameter_values(pipeline.parameters, shader_material_defaults(recipe));
        const auto validate_literal = [](const ShaderLiteral& literal) {
            const auto finite = std::visit([](const auto& value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_arithmetic_v<T>)
                    return std::isfinite(value);
                else if constexpr (std::is_same_v<T, glm::mat4>) {
                    for (glm::length_t column = 0; column < 4; ++column)
                        for (glm::length_t row = 0; row < 4; ++row)
                            if (!std::isfinite(value[column][row]))
                                return false;
                } else {
                    for (glm::length_t i = 0; i < value.length(); ++i)
                        if (!std::isfinite(value[i]))
                            return false;
                }
                return true;
            }, literal);
            if (!finite)
                throw Exceptions::invalid_args(CE_HERE, "Shader literals must be finite");
        };
        for (const auto& parameter : recipe.parameters)
            if (parameter.default_value)
                validate_literal(*parameter.default_value);
        for (const auto& [key, value] : recipe.defaults) {
            static_cast<void>(key);
            validate_literal(value);
        }
        for (const auto& [key, options] : recipe.sampling) {
            const auto parameter = std::ranges::find(recipe.parameters, key, &ShaderParameter::key);
            if (parameter == recipe.parameters.end() || parameter->type != ParameterType::Sampler2D)
                throw Exceptions::invalid_args(CE_HERE, "FX sampling policy requires a sampler parameter '" + key + "'");
            validate_sampler_options(options);
        }
    }
}
