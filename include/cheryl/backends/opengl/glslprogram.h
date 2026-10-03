#pragma once
#ifndef GLSLPROGRAM_H
#define GLSLPROGRAM_H

#define GLM_ENABLE_EXPERIMENTAL
#include <assets/resources/shader.h>

#include <backends/opengl/gl.h>
#include <backends/opengl/resource-lifetime.h>
#include <cassert>
#include <glm.hpp>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

namespace CE::Assets {
    struct GLSLVariable {
        std::string name;
        GLenum type = 0;
        GLint size = 0;
        GLint location = -1;
    };

    // GLSL naming belongs to this backend. Empty names omit roles a program does not use.
    struct GLSLMaterialBindings {
        std::string projection = "projectionMatrix";
        std::string view = "viewMatrix";
        std::string model = "modelMatrix";
        std::string alpha = "in_Alpha";
        std::string scale = "in_Scale";
        std::string texture = "mytexture";
    };

    struct GLSLProgram final : Shader {
    private:
        RenderAPIs::OpenGLHandle program_;
        GLSLMaterialBindings material_bindings_;
        std::map<std::string, int> uniforms_;
        std::map<std::string, int> attributes_;

    public:
        explicit GLSLProgram(RenderAPIs::OpenGLHandle program);
        void use() override;
        void bind_pass(const ShaderPass& pass) override;
        void bind_draw(const ShaderDraw& draw) override;
        void set_material_bindings(GLSLMaterialBindings bindings);
        void require_current() const { (void)program_.id(); }
        void require_linked() const;
        [[nodiscard]] const RenderAPIs::OpenGLResourceLifetime* resource_domain() const noexcept { return program_.resource_domain(); }
        [[nodiscard]] std::vector<GLSLVariable> active_uniforms() const;
        [[nodiscard]] std::vector<GLSLVariable> active_attributes() const;

        template <glm::length_t dim> void set_uniform_vec(const char* name, const glm::vec<dim, glm::f32, glm::defaultp>& v);
        template <glm::length_t dim> void set_uniform_matrix(const char* name, const glm::mat<dim, dim, glm::f32, glm::defaultp>& m);
        template <typename T> void set_uniform_value(const char* name, const T& v);

        void set_uniform_value(const char* name, float value) override { set_uniform_value<float>(name, value); }
        void set_uniform_value(const char* name, int value) override { set_uniform_value<int>(name, value); }
        void set_uniform_value(const char* name, unsigned int value) override { set_uniform_value<unsigned int>(name, value); }
        void set_uniform_value(const char* name, bool value) override { set_uniform_value<bool>(name, value); }
        void set_uniform_matrix(const char* name, const glm::mat4& value) override { set_uniform_matrix<4>(name, value); }

        // todo: convert the code from both methods into parsers that register events
        void print_active_uniforms() const;
        void print_active_attribs() const;

        int get_uniform_location(const char* name);
        int get_attribute_location(const char* name);
    };

    template <glm::length_t dim> void GLSLProgram::set_uniform_vec(const char* name, const glm::vec<dim, glm::f32, glm::defaultp>& v) {
        // Resolve a linked program's uniform once, then choose the matching
        // GL upload at compile time from the vector's dimension.
        static_assert(2 <= dim && dim <= 4, "set_uniform_vec can only take 2-4D vectors");
        int loc = get_uniform_location(name);
        assert(loc >= 0 && "set_uniform_vec failed");
        if (loc >= 0) {
            if constexpr (dim == 2) {
                glUniform2f(loc, v.x, v.y);
            } else if constexpr (dim == 3) {
                glUniform3f(loc, v.x, v.y, v.z);
            } else if constexpr (dim == 4) {
                glUniform4f(loc, v.x, v.y, v.z, v.w);
            }
        }
    }

    template <glm::length_t dim>
    void GLSLProgram::set_uniform_matrix(const char* name, const glm::mat<dim, dim, glm::f32, glm::defaultp>& m) {
        // GLM's contiguous column-major storage is passed from its first
        // element; the dimension selects the appropriate matrix uniform call.
        static_assert(2 <= dim && dim <= 4, "set_uniform_matrix can only take 2-4D matrices");
        int loc = get_uniform_location(name);
        assert(loc >= 0 && "set_uniform_matrix failed");
        if (loc >= 0) {
            if constexpr (dim == 2) {
                glUniformMatrix2fv(loc, 1, 0, &m[0][0]);
            } else if constexpr (dim == 3) {
                glUniformMatrix3fv(loc, 1, 0, &m[0][0]);
            } else if constexpr (dim == 4) {
                glUniformMatrix4fv(loc, 1, 0, &m[0][0]);
            }
        }
    }

    template <typename T> void GLSLProgram::set_uniform_value(const char* name, const T& v) {
        static_assert(
            std::is_same_v<T, GLfloat> || std::is_same_v<T, GLuint> || std::is_same_v<T, GLint> || std::is_same_v<T, bool>,
            "set_uniform_value must take a float, int, or bool"
        );
        int loc = get_uniform_location(name);
        assert(loc >= 0 && "set_uniform_value failed");
        if (loc >= 0) {
            if constexpr (std::is_same_v<T, GLfloat>) {
                glUniform1f(loc, v);
            } else if constexpr (std::is_same_v<T, GLuint>) {
                glUniform1ui(loc, v);
            } else if constexpr (std::is_same_v<T, GLint>) {
                glUniform1i(loc, v);
            } else if constexpr (std::is_same_v<T, bool>) {
                glUniform1i(loc, v);
            }
        }
    }
}
/* Important: no changelog will be updated Version 2.0 was quite a while ago
 * ..and that is why we're at the bottom of the file
 *
 * GLSLProgram is based on code from  the excellent text "OpenGL 4.0 Shading Language Cookbook"
    by David Wolff.
    Modified by Darren Reid to suit Blit3D needs.

    Version 2.0 replaced functions with compile time templates
    Version 1.1 added support for vec2 uniforms
    Version 1.0	added a map for uniform/attributes, to cache lookup of locations in shader
*/
#endif
