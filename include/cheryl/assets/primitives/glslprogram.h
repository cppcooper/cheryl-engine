#pragma once
#ifndef GLSLPROGRAM_H
#define GLSLPROGRAM_H

#define GLM_ENABLE_EXPERIMENTAL
#include <cgl.h>
#include <glm.hpp>
#include <string>
#include <map>

using GLboolean = unsigned char;
using GLbyte = signed char;
using GLubyte = unsigned char;
using GLshort = short;
using GLushort = unsigned short;
using GLint = int;
using GLuint = unsigned int;
using GLsizei = int;
using GLfloat = float;
using GLdouble = double;
using GLchar = char;

namespace CE::Assets {
    // todo: move abstract to new header, refactor name probably, maybe add new methods
    struct Shader {
        virtual ~Shader() = default;

        virtual void use() = 0;
    };
    struct GLSLProgram final : Shader {
        explicit GLSLProgram(int program_id);
        ~GLSLProgram() override;
        void use() override;

        void bind_attrib_location(GLuint location, const char* name) const;
        void bind_frag_data_location(GLuint location, const char* name) const;

        template<glm::length_t dim>
        void set_uniform_vec(const char* name, const glm::vec<dim, glm::f32, glm::defaultp>& v);
        template<glm::length_t dim>
        void set_uniform_matrix(const char* name, const glm::mat<dim, dim, glm::f32, glm::defaultp>& m);
        template<typename T>
        void set_uniform_value(const char* name, const T& v);

        // todo: convert the code from both methods into parsers that register events
        void print_active_uniforms() const;
        void print_active_attribs() const;

        int get_uniform_location(const char* name);
        int get_attribute_location(const char* name);
    private:
        int id_prog;
        bool linked;
        //Store uniforms and attributes in a map for easy lookup
        std::map<std::string, int> locations;

        bool link();

    };

    template<glm::length_t dim>
    void GLSLProgram::set_uniform_vec(const char* name, const glm::vec<dim, glm::f32, glm::defaultp>& v) {
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

    template<glm::length_t dim>
    void GLSLProgram::set_uniform_matrix(const char* name, const glm::mat<dim, dim, glm::f32, glm::defaultp>& m) {
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

    template<typename T>
    void GLSLProgram::set_uniform_value(const char* name, const T& v) {
        static_assert(std::is_same_v<T, GLfloat> || std::is_same_v<T, GLuint> || std::is_same_v<T, GLint> || std::is_same_v<T, bool>, "set_uniform_value must take a float, int, or bool");
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
