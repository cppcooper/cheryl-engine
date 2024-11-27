#pragma once
#ifndef GLSLPROGRAM_H
#define GLSLPROGRAM_H

#define GLM_ENABLE_EXPERIMENTAL
#include <cgl.h>
#include <glm.hpp>
#include <string>
#include <map>

namespace GLSLShader {
    enum GLSLShaderType {
        VERTEX, FRAGMENT, GEOMETRY,
        TESS_CONTROL, TESS_EVALUATION
    };

    inline GLSLShaderType get_type(const std::string& extension) {
        if (extension == ".vert") {
            return VERTEX;
        }
        if (extension == ".frag") {
            return FRAGMENT;
        }
        if (extension == ".geo") {
            return GEOMETRY;
        }
        if (extension == ".tesc") {
            return TESS_CONTROL;
        }
        if (extension == ".tese") {
            return TESS_EVALUATION;
        }
        return FRAGMENT;  // Default case
    }
};
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

class GLSLProgram {
    int id_prog;
    bool linked;
    std::string log_info;

    int getUniformLocation(const char* name);

    //std::map<int,int> mymap;
    //Store uniforms and attributes in a map for easy lookup
    std::map<std::string, int> UniformMap;
    std::map<std::string, int>::iterator UMapIter;

public:
    GLSLProgram();
    ~GLSLProgram();

    bool compileShaderFromFile(const char* fileName, GLSLShader::GLSLShaderType type);
    bool compileShaderFromString(const std::string &source, GLSLShader::GLSLShaderType type);
    bool link();
    void use();

    std::string log();

    int getHandle();
    bool isLinked();

    void bindAttribLocation(GLuint location, const char* name);
    void bindFragDataLocation(GLuint location, const char* name);

    template<glm::length_t dim>
    void set_uniform_vec(const char* name, const glm::vec<dim, glm::f32, glm::defaultp>& v);
    template<glm::length_t dim>
    void set_uniform_matrix(const char* name, const glm::mat<dim, dim, glm::f32, glm::defaultp>& m);
    template<typename T>
    void set_uniform_value(const char* name, const T& v);

    void printActiveUniforms();
    void printActiveAttribs();

    int GetUniform(const char* name);
    int GetAttribute(const char* name);
};

template<glm::length_t dim>
void GLSLProgram::set_uniform_vec(const char* name, const glm::vec<dim, glm::f32, glm::defaultp>& v) {
    static_assert(2 <= dim && dim <= 4, "set_uniform_vec can only take 2-4D vectors");
    int loc = getUniformLocation(name);
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
    int loc = getUniformLocation(name);
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
    int loc = getUniformLocation(name);
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
