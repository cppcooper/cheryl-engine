#include <backends/opengl/pipeline.h>

#include <backends/opengl/texture.h>
#include <backends/opengl/sampler.h>
#include <backends/opengl/vertex-array-object.h>
#include <internals/exceptions.h>

#include <algorithm>
#include <map>
#include <set>
#include <type_traits>
#include <utility>

namespace CE::Rendering::OpenGL {
    namespace {
        GLenum uniform_type(const CE::Rendering::Parameters::Type type) {
            switch (type) {
                case CE::Rendering::Parameters::Type::Float:
                    return GL_FLOAT;
                case CE::Rendering::Parameters::Type::Int:
                    return GL_INT;
                case CE::Rendering::Parameters::Type::UInt:
                    return GL_UNSIGNED_INT;
                case CE::Rendering::Parameters::Type::Bool:
                    return GL_BOOL;
                case CE::Rendering::Parameters::Type::Vec2:
                    return GL_FLOAT_VEC2;
                case CE::Rendering::Parameters::Type::Vec3:
                    return GL_FLOAT_VEC3;
                case CE::Rendering::Parameters::Type::Vec4:
                    return GL_FLOAT_VEC4;
                case CE::Rendering::Parameters::Type::Mat4:
                    return GL_FLOAT_MAT4;
                case CE::Rendering::Parameters::Type::Sampler2D:
                    return GL_SAMPLER_2D;
            }
            throw Exceptions::invalid_args(CE_HERE, "Unsupported GLSL parameter type");
        }

        void validate_reset_value(const CE::Rendering::Parameters::Definition& definition, const CE::Rendering::Parameters::Value& value) {
            if (definition.required || definition.semantic != CE::Rendering::Parameters::Semantic::Custom ||
                CE::Rendering::Parameters::type_of(value) != definition.type)
                throw Exceptions::invalid_args(CE_HERE, "Invalid optional uniform reset: " + definition.key);
            if (const auto* image = std::get_if<CE::Rendering::Parameters::ImageBinding>(&value); image && !image->image)
                throw Exceptions::invalid_args(CE_HERE, "Optional sampler reset needs an image: " + definition.key);
        }

        void upload_parameter(const GLint location, const CE::Rendering::Parameters::Value& value) {
            std::visit(
                [location](const auto& item) {
                    using T = std::decay_t<decltype(item)>;
                    if constexpr (std::is_same_v<T, float>)
                        glUniform1f(location, item);
                    else if constexpr (std::is_same_v<T, int>)
                        glUniform1i(location, item);
                    else if constexpr (std::is_same_v<T, unsigned int>)
                        glUniform1ui(location, item);
                    else if constexpr (std::is_same_v<T, bool>)
                        glUniform1i(location, item ? 1 : 0);
                    else if constexpr (std::is_same_v<T, glm::vec2>)
                        glUniform2f(location, item.x, item.y);
                    else if constexpr (std::is_same_v<T, glm::vec3>)
                        glUniform3f(location, item.x, item.y, item.z);
                    else if constexpr (std::is_same_v<T, glm::vec4>)
                        glUniform4f(location, item.x, item.y, item.z, item.w);
                    else if constexpr (std::is_same_v<T, glm::mat4>)
                        glUniformMatrix4fv(location, 1, GL_FALSE, &item[0][0]);
                    else {
                        item.image->bind(item.unit);
                        if (item.sampler)
                            item.sampler->bind(item.unit);
                        glUniform1i(location, static_cast<GLint>(item.unit));
                    }
                },
                value
            );
        }
    }

    Pipeline::Pipeline(CE::Rendering::Pipelines::Definition definition, std::shared_ptr<Program> program, const PipelineBindings& bindings)
    : CE::Rendering::Resources::Pipeline(std::move(definition)), program_(std::move(program)) {
        if (!program_)
            throw Exceptions::invalid_args(CE_HERE, "GLSL pipeline needs a linked program");
        program_->require_linked();
        const auto& contract = this->definition().parameters;
        std::map<std::string, const ParameterBinding*, std::less<>> mappings;
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
            const bool position =
                attribute.name == bindings.position_attribute && attribute.location == 0 && attribute.type == GL_FLOAT_VEC3;
            const bool uv = attribute.name == bindings.uv_attribute && attribute.location == 1 && attribute.type == GL_FLOAT_VEC2;
            const bool color = this->definition().vertex_layout == CE::Rendering::Geometry::Layout2D::Position3UV2Color4 &&
                               attribute.name == bindings.color_attribute && attribute.location == 2 && attribute.type == GL_FLOAT_VEC4;
            if ((!position && !uv && !color) || attribute.size != 1 || attribute.name.find('[') != std::string::npos)
                throw Exceptions::invalid_args(CE_HERE, "Program attribute does not match the selected 2D layout: " + attribute.name);
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
            const auto uniform =
                std::find_if(uniforms.begin(), uniforms.end(), [&](const auto& item) { return item.name == binding.uniform; });
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
            if (!definition.required && definition.semantic == CE::Rendering::Parameters::Semantic::Custom && !missing_value)
                throw Exceptions::invalid_args(CE_HERE, "Active optional GLSL uniform needs a default/reset: " + binding.uniform);
            parameters_.push_back({definition.key, uniform->location, std::move(missing_value)});
        }
        if (consumed.size() != uniforms.size())
            throw Exceptions::invalid_args(CE_HERE, "Every active GLSL uniform must belong to the parameter contract");
        CE::Rendering::Parameters::Set defaults;
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

    void Pipeline::validate_resources(const CE::Rendering::Parameters::Set& values) const {
        program_->require_current();
        for (const auto& [key, value] : values) {
            const auto* binding = std::get_if<CE::Rendering::Parameters::ImageBinding>(&value);
            if (!binding)
                continue;
            const auto* texture = dynamic_cast<const Texture*>(binding->image.get());
            if (!texture || texture->resource_domain() != resource_domain())
                throw Exceptions::invalid_args(CE_HERE, "Sampler image does not belong to the pipeline's native domain: " + key);
            texture->require_binding(binding->unit);
            if (binding->sampler) {
                const auto* sampler = dynamic_cast<const Sampler*>(binding->sampler.get());
                if (!sampler || sampler->resource_domain() != resource_domain())
                    throw Exceptions::invalid_args(CE_HERE, "Sampler does not belong to the pipeline's native domain: " + key);
                sampler->require_binding(binding->unit);
                if (sampler->options().mipmaps != CE::Rendering::Sampling::MipmapFilter::None && !texture->has_complete_mipmaps())
                    throw Exceptions::invalid_args(CE_HERE, "Sampler requires a complete image mipmap chain: " + key);
            }
        }
    }

    CE::Rendering::Parameters::Set Pipeline::prepare_parameters(const CE::Rendering::Parameters::Set& values) const {
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
        CE::Rendering::Parameters::validate_resolved(definition().parameters, effective);
        validate_resources(effective);
        return effective;
    }

    void Pipeline::apply_parameters(const CE::Rendering::Parameters::Set& values) const {
        // No program/texture state changes occur until the complete request passes.
        program_->use();
        for (const auto& parameter : parameters_)
            upload_parameter(parameter.location, values.at(parameter.key));
    }

    void Pipeline::bind_parameters(const CE::Rendering::Parameters::Set& values) const {
        apply_parameters(prepare_parameters(values));
    }

    void Pipeline::apply_fixed_state() const {
        const auto& state = definition().state;
        if (state.blend == CE::Rendering::Pipelines::Blend::Opaque)
            glDisable(GL_BLEND);
        else
            glEnable(GL_BLEND);
        glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
        switch (state.blend) {
            case CE::Rendering::Pipelines::Blend::Opaque:
                glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_ONE, GL_ZERO);
                break;
            case CE::Rendering::Pipelines::Blend::StraightAlpha:
                glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                break;
            case CE::Rendering::Pipelines::Blend::PremultipliedAlpha:
                glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                break;
            case CE::Rendering::Pipelines::Blend::Additive:
                glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE);
                break;
        }
        if (state.depth == CE::Rendering::Pipelines::Depth::Disabled)
            glDisable(GL_DEPTH_TEST);
        else
            glEnable(GL_DEPTH_TEST);
        glDepthFunc(state.depth == CE::Rendering::Pipelines::Depth::LessEqual ? GL_LEQUAL : GL_LESS);
        glDepthMask(state.depth_write ? GL_TRUE : GL_FALSE);
        if (state.cull == CE::Rendering::Pipelines::Cull::None)
            glDisable(GL_CULL_FACE);
        else
            glEnable(GL_CULL_FACE);
        glFrontFace(GL_CCW);
        glCullFace(state.cull == CE::Rendering::Pipelines::Cull::Front ? GL_FRONT : GL_BACK);
    }

    void Pipeline::draw(
        const CE::Rendering::Geometry::Buffer2D& geometry,
        const std::size_t first_vertex,
        const std::size_t vertex_count,
        const CE::Rendering::Parameters::Set& values,
        const CE::Rendering::Pipelines::PassConstraints2D& constraints
    ) const {
        draw(geometry, first_vertex, vertex_count, values, constraints, {}, {0, 0});
    }

    void Pipeline::draw(
        const CE::Rendering::Geometry::Buffer2D& geometry,
        const std::size_t first_vertex,
        const std::size_t vertex_count,
        const CE::Rendering::Parameters::Set& values,
        const CE::Rendering::Pipelines::PassConstraints2D& constraints,
        const std::optional<ClipRegion2D>& clip,
        const CE::Platform::FramebufferSize framebuffer
    ) const {
        validate_draw(geometry, first_vertex, vertex_count, constraints);
        const auto* vao = dynamic_cast<const CE::Rendering::OpenGL::VertexArray*>(&geometry);
        if (!vao || vao->resource_domain() != resource_domain())
            throw Exceptions::invalid_args(CE_HERE, "Geometry does not belong to the pipeline's native domain");
        vao->require_draw(first_vertex, vertex_count);
        const auto effective = prepare_parameters(values);
        const auto pixels = clip ? std::optional{resolve_clip_region(*clip, framebuffer)} : std::nullopt;
        if (pixels && pixels->empty())
            return;
        // Reapply every supported setting, including disabled-state parameters.
        // Adjacent draws and passes cannot inherit blend/depth/cull policy.
        if (pixels) {
            glEnable(GL_SCISSOR_TEST);
            glScissor(pixels->left, framebuffer.height - pixels->bottom, pixels->right - pixels->left, pixels->bottom - pixels->top);
        } else {
            glDisable(GL_SCISSOR_TEST);
        }
        apply_fixed_state();
        apply_parameters(effective);
        vao->bind();
        vao->draw(first_vertex, vertex_count);
    }
}
