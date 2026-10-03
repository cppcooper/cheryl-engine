#include <backends/opengl/glslprogram.h>
#include "upload-check.h"
#include <internals/exceptions.h>
#include <internals/compile-time-logging.hpp>
#include <utility>

namespace CE::Assets {
    namespace {
        std::vector<GLSLVariable> reflect_variables(
            const GLuint program,
            const GLenum count_parameter,
            const GLenum length_parameter,
            decltype(glad_glGetActiveUniform) query,
            decltype(glad_glGetUniformLocation) location
        ) {
            RenderAPIs::require_no_gl_error("OpenGL error before program reflection");
            GLint count = 0;
            GLint maximum_length = 0;
            glGetProgramiv(program, count_parameter, &count);
            RenderAPIs::require_no_gl_error("Could not query program reflection count");
            glGetProgramiv(program, length_parameter, &maximum_length);
            RenderAPIs::require_no_gl_error("Could not query program reflection name length");
            if (count < 0 || (count > 0 && maximum_length <= 0))
                throw Exceptions::failed_operation(CE_HERE, "Invalid linked program reflection limits");
            if (count == 0)
                return {};
            std::vector<GLchar> name(static_cast<std::size_t>(maximum_length));
            std::vector<GLSLVariable> variables;
            variables.reserve(static_cast<std::size_t>(count));
            for (GLint index = 0; index < count; ++index) {
                GLSLVariable variable;
                GLsizei written = 0;
                query(program, static_cast<GLuint>(index), maximum_length, &written, &variable.size, &variable.type, name.data());
                RenderAPIs::require_no_gl_error("Could not reflect program variable");
                if (written <= 0 || written >= maximum_length)
                    throw Exceptions::failed_operation(CE_HERE, "Invalid linked program reflection name");
                variable.name.assign(name.data(), static_cast<std::size_t>(written));
                variable.location = location(program, variable.name.c_str());
                RenderAPIs::require_no_gl_error("Could not query reflected variable location");
                variables.push_back(std::move(variable));
            }
            return variables;
        }
    }

    GLSLProgram::GLSLProgram(RenderAPIs::OpenGLHandle program)
    : program_(std::move(program)) {
        (void)program_.id();
        if (program_.kind() != RenderAPIs::GLResourceKind::Program)
            throw Exceptions::invalid_args(CE_HERE, "GLSLProgram requires a tracked program handle");
    }

    void GLSLProgram::use() {
        glUseProgram(program_.id());
    }

    void GLSLProgram::require_linked() const {
        GLint linked = GL_FALSE;
        const auto id = program_.id();
        RenderAPIs::require_no_gl_error("OpenGL error before program link-status query");
        glGetProgramiv(id, GL_LINK_STATUS, &linked);
        RenderAPIs::require_no_gl_error("Could not query program link status");
        if (linked != GL_TRUE)
            throw Exceptions::failed_operation(CE_HERE, "GLSL pipeline requires a successfully linked program");
    }

    std::vector<GLSLVariable> GLSLProgram::active_uniforms() const {
        return reflect_variables(program_.id(), GL_ACTIVE_UNIFORMS, GL_ACTIVE_UNIFORM_MAX_LENGTH, glGetActiveUniform, glGetUniformLocation);
    }

    std::vector<GLSLVariable> GLSLProgram::active_attributes() const {
        return reflect_variables(
            program_.id(), GL_ACTIVE_ATTRIBUTES, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, glGetActiveAttrib, glGetAttribLocation
        );
    }

    void GLSLProgram::set_material_bindings(GLSLMaterialBindings bindings) {
        (void)program_.id();
        material_bindings_ = std::move(bindings);
    }

    void GLSLProgram::bind_pass(const ShaderPass& pass) {
        use();
        if (!material_bindings_.projection.empty())
            set_uniform_matrix(material_bindings_.projection.c_str(), pass.projection);
        if (!material_bindings_.view.empty())
            set_uniform_matrix(material_bindings_.view.c_str(), pass.view);
    }

    void GLSLProgram::bind_draw(const ShaderDraw& draw) {
        if (!material_bindings_.model.empty())
            set_uniform_matrix(material_bindings_.model.c_str(), draw.model);
        if (!material_bindings_.alpha.empty())
            set_uniform_value(material_bindings_.alpha.c_str(), draw.alpha);
        if (!material_bindings_.scale.empty())
            set_uniform_value(material_bindings_.scale.c_str(), draw.scale);
        if (!material_bindings_.texture.empty())
            set_uniform_value(material_bindings_.texture.c_str(), draw.texture_unit);
    }

    void GLSLProgram::print_active_uniforms() const {
        const auto variables = active_uniforms();
        CE_LOG_DEBUG(CE::enginelog, "subsystem=shader domain={} operation=reflection kind=uniforms count={}", domain_, variables.size());
        for (const auto& variable : variables)
            CE_LOG_TRACE(CE::enginelog, "subsystem=shader domain={} operation=uniform name={} location={} type={} size={}",
                         domain_, variable.name, variable.location, variable.type, variable.size);
    }

    void GLSLProgram::print_active_attribs() const {
        const auto variables = active_attributes();
        CE_LOG_DEBUG(CE::enginelog, "subsystem=shader domain={} operation=reflection kind=attributes count={}", domain_, variables.size());
        for (const auto& variable : variables)
            CE_LOG_TRACE(CE::enginelog, "subsystem=shader domain={} operation=attribute name={} location={} type={} size={}",
                         domain_, variable.name, variable.location, variable.type, variable.size);
    }

    int GLSLProgram::get_uniform_location(const char* name) {
        // Query OpenGL once after linking, caching valid locations for repeated draw calls.
        const auto id_prog = program_.id();
        if (!name)
            throw Exceptions::invalid_args(CE_HERE, "Program variable name must not be null");
        if (const auto it = uniforms_.find(name); it != uniforms_.end())
            return it->second;
        RenderAPIs::require_no_gl_error("OpenGL error before uniform location query");
        const int result = glGetUniformLocation(id_prog, name);
        RenderAPIs::require_no_gl_error("Could not query uniform location");
        uniforms_.emplace(name, result);
        return result;
    }

    int GLSLProgram::get_attribute_location(const char* name) {
        const auto id_prog = program_.id();
        if (!name)
            throw Exceptions::invalid_args(CE_HERE, "Program variable name must not be null");
        if (const auto it = attributes_.find(name); it != attributes_.end())
            return it->second;
        RenderAPIs::require_no_gl_error("OpenGL error before attribute location query");
        const int result = glGetAttribLocation(id_prog, name);
        RenderAPIs::require_no_gl_error("Could not query attribute location");
        attributes_.emplace(name, result);
        return result;
    }
}
