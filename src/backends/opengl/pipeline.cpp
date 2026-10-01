#include <backends/opengl/pipeline.h>

#include <backends/opengl/texture.h>
#include <internals/exceptions.h>

#include <algorithm>
#include <map>
#include <set>
#include <type_traits>
#include <utility>

namespace CE::Assets {
    namespace {
        GLenum uniform_type(const ParameterType type) {
            switch (type) {
                case ParameterType::Float: return GL_FLOAT;
                case ParameterType::Int: return GL_INT;
                case ParameterType::UInt: return GL_UNSIGNED_INT;
                case ParameterType::Bool: return GL_BOOL;
                case ParameterType::Vec2: return GL_FLOAT_VEC2;
                case ParameterType::Vec3: return GL_FLOAT_VEC3;
                case ParameterType::Vec4: return GL_FLOAT_VEC4;
                case ParameterType::Mat4: return GL_FLOAT_MAT4;
                case ParameterType::Sampler2D: return GL_SAMPLER_2D;
            }
            throw Exceptions::invalid_args(CE_HERE, "Unsupported GLSL parameter type");
        }

        void validate_reset_value(
            const ParameterDefinition& definition,
            const ParameterValue& value
        ) {
            if (definition.required || definition.semantic != ParameterSemantic::Custom || parameter_type(value) != definition.type)
                throw Exceptions::invalid_args(CE_HERE, "Invalid optional uniform reset: " + definition.key);
            if (const auto* image = std::get_if<ImageBinding>(&value); image && !image->image)
                throw Exceptions::invalid_args(CE_HERE, "Optional sampler reset needs an image: " + definition.key);
        }

        void upload_parameter(
            const GLint location,
            const ParameterValue& value
        ) {
            std::visit([location](const auto& item) {
                using T = std::decay_t<decltype(item)>;
                if constexpr (std::is_same_v<T, float>) glUniform1f(location, item);
                else if constexpr (std::is_same_v<T, int>) glUniform1i(location, item);
                else if constexpr (std::is_same_v<T, unsigned int>) glUniform1ui(location, item);
                else if constexpr (std::is_same_v<T, bool>) glUniform1i(location, item ? 1 : 0);
                else if constexpr (std::is_same_v<T, glm::vec2>) glUniform2f(location, item.x, item.y);
                else if constexpr (std::is_same_v<T, glm::vec3>) glUniform3f(location, item.x, item.y, item.z);
                else if constexpr (std::is_same_v<T, glm::vec4>) glUniform4f(location, item.x, item.y, item.z, item.w);
                else if constexpr (std::is_same_v<T, glm::mat4>) glUniformMatrix4fv(location, 1, GL_FALSE, &item[0][0]);
                else {
                    item.image->bind(item.unit);
                    glUniform1i(location, static_cast<GLint>(item.unit));
                }
            }, value);
        }
    }

    GLSLPipeline::GLSLPipeline(
        PipelineDefinition definition,
        std::shared_ptr<GLSLProgram> program,
        const GLSLPipelineBindings& bindings
    )
    : Pipeline(std::move(definition)), program_(std::move(program)) {
        if (!program_)
            throw Exceptions::invalid_args(CE_HERE, "GLSL pipeline needs a linked program");
        program_->require_linked();
        const auto& contract = this->definition().parameters;
        std::map<std::string, const GLSLParameterBinding*, std::less<>> mappings;
        std::set<std::string> uniform_names;
        for (const auto& binding : bindings.parameters) {
            if (binding.key.empty() || binding.uniform.empty() || !mappings.emplace(binding.key, &binding).second ||
                !uniform_names.insert(binding.uniform).second)
                throw Exceptions::invalid_args(CE_HERE, "GLSL parameter mappings must have unique nonempty keys/names");
            if (std::none_of(contract.begin(), contract.end(), [&](const auto& item) { return item.key == binding.key; }))
                throw Exceptions::invalid_args(CE_HERE, "GLSL mapping is outside the parameter contract: " + binding.key);
        }
        const auto uniforms = program_->active_uniforms();
        const auto attributes = program_->active_attributes();
        for (const auto& attribute : attributes) {
            const bool position = attribute.name == bindings.position_attribute && attribute.location == 0 && attribute.type == GL_FLOAT_VEC3;
            const bool uv = attribute.name == bindings.uv_attribute && attribute.location == 1 && attribute.type == GL_FLOAT_VEC2;
            if ((!position && !uv) || attribute.size != 1 || attribute.name.find('[') != std::string::npos)
                throw Exceptions::invalid_args(CE_HERE, "Program attribute does not match Vertex2D: " + attribute.name);
        }
        std::set<std::string> consumed;
        parameters_.reserve(contract.size());
        for (const auto& definition : contract) {
            const auto mapping = mappings.find(definition.key);
            if (mapping == mappings.end())
                throw Exceptions::invalid_args(CE_HERE, "Parameter needs an explicit GLSL mapping: " + definition.key);
            const auto& binding = *mapping->second;
            if (binding.missing_value)
                validate_reset_value(definition, *binding.missing_value);
            const auto uniform = std::find_if(uniforms.begin(), uniforms.end(), [&](const auto& item) { return item.name == binding.uniform; });
            if (uniform == uniforms.end()) {
                if (definition.required)
                    throw Exceptions::invalid_args(CE_HERE, "Required GLSL uniform is inactive/missing: " + binding.uniform);
                continue;
            }
            if (uniform->size != 1 || uniform->location < 0 || uniform->name.find('[') != std::string::npos ||
                uniform->type != uniform_type(definition.type))
                throw Exceptions::invalid_args(CE_HERE, "GLSL uniform type/storage does not match its contract: " + binding.uniform);
            consumed.insert(uniform->name);
            auto missing_value = definition.default_value ? definition.default_value : binding.missing_value;
            if (!definition.required && definition.semantic == ParameterSemantic::Custom && !missing_value)
                throw Exceptions::invalid_args(CE_HERE, "Active optional GLSL uniform needs a default/reset: " + binding.uniform);
            parameters_.push_back({definition.key, uniform->location, std::move(missing_value)});
        }
        if (consumed.size() != uniforms.size())
            throw Exceptions::invalid_args(CE_HERE, "Every active GLSL uniform must belong to the parameter contract");
        ParameterSet defaults;
        for (const auto& definition : contract) {
            if (definition.default_value)
                defaults.emplace(definition.key, *definition.default_value);
        }
        validate_resources(defaults);
        for (const auto& parameter : parameters_) {
            if (parameter.missing_value)
                validate_resources({{parameter.key, *parameter.missing_value}});
        }
    }

    void GLSLPipeline::validate_resources(const ParameterSet& values) const {
        program_->require_current();
        for (const auto& [key, value] : values) {
            const auto* binding = std::get_if<ImageBinding>(&value);
            if (!binding)
                continue;
            const auto* texture = dynamic_cast<const Texture*>(binding->image.get());
            if (!texture || texture->resource_domain() != resource_domain())
                throw Exceptions::invalid_args(CE_HERE, "Sampler image does not belong to the pipeline's native domain: " + key);
            texture->require_binding(binding->unit);
        }
    }

    void GLSLPipeline::bind_parameters(const ParameterSet& values) const {
        program_->require_current();
        auto effective = values;
        // Fill absent active optional values before validation, so reset samplers
        // participate in the same unit/domain checks as authored draw resources.
        for (const auto& parameter : parameters_) {
            if (!effective.contains(parameter.key)) {
                if (parameter.missing_value)
                    effective.emplace(parameter.key, *parameter.missing_value);
                else
                    throw Exceptions::invalid_args(CE_HERE, "Active GLSL parameter is missing: " + parameter.key);
            }
        }
        validate_resolved_parameters(definition().parameters, effective);
        validate_resources(effective);
        // No program/texture state changes occur until the complete request passes.
        program_->use();
        for (const auto& parameter : parameters_)
            upload_parameter(parameter.location, effective.at(parameter.key));
    }
}
