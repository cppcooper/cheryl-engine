
#include <backends/opengl/glslprogram.h>
#include <cstdlib>
#include <iostream>
#include <utility>

namespace CE::Assets {
    GLSLProgram::GLSLProgram(std::shared_ptr<RenderAPIs::OpenGLResourceLifetime> lifetime, const GLuint program_id)
        : program_(std::move(lifetime), RenderAPIs::GLResourceKind::Program, program_id) {}

    void GLSLProgram::use() { glUseProgram(program_.id()); }

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

        GLint nUniforms, size, maxLen;
        GLsizei written;
        GLenum type;

        const auto id_prog = program_.id();
        glGetProgramiv(id_prog, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxLen);
        glGetProgramiv(id_prog, GL_ACTIVE_UNIFORMS, &nUniforms);

        // todo: replace malloc/free
        const auto name = static_cast<GLchar*>(malloc(maxLen));

        std::cout << " Location | Name\n";
        std::cout << "------------------------------------------------\n";
        for (int i = 0; i < nUniforms; ++i) {
            glGetActiveUniform(id_prog, i, maxLen, &written, &size, &type, name);
            const GLint location = glGetUniformLocation(id_prog, name);
            std::cout << location << " | " << name << "\n";
        }

        free(name);
    }

    void GLSLProgram::print_active_attribs() const {

        GLint written, size, maxLength, nAttribs;
        GLenum type;

        const auto id_prog = program_.id();
        glGetProgramiv(id_prog, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxLength);
        glGetProgramiv(id_prog, GL_ACTIVE_ATTRIBUTES, &nAttribs);

        const auto name = static_cast<GLchar*>(malloc(maxLength));

        std::cout << " Index | Name\n";
        std::cout << "------------------------------------------------\n";
        for (int i = 0; i < nAttribs; i++) {
            glGetActiveAttrib(id_prog, i, maxLength, &written, &size, &type, name);
            const GLint location = glGetAttribLocation(id_prog, name);
            std::cout << location << " | " << name << "\n";
        }

        free(name);
    }

    int GLSLProgram::get_uniform_location(const char* name) {
        // Query OpenGL once after linking, caching valid locations for repeated draw calls.
        const auto id_prog = program_.id();
        if (const auto it = uniforms_.find(name); it != uniforms_.end())
            return it->second;
        const int result = glGetUniformLocation(id_prog, name);
        uniforms_.emplace(name, result);
        return result;
    }

    int GLSLProgram::get_attribute_location(const char* name) {
        const auto id_prog = program_.id();
        if (const auto it = attributes_.find(name); it != attributes_.end())
            return it->second;
        const int result = glGetAttribLocation(id_prog, name);
        attributes_.emplace(name, result);
        return result;
    }
}
