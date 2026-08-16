
#include <assets/primitives/glslprogram.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <internals/celog.h>
#include <internals/exceptions.h>

using std::ifstream;
using std::ios;

#include <sstream>

using std::ostringstream;
namespace fs = std::filesystem;

namespace CE::Assets {
    GLSLProgram::GLSLProgram(int program_id) : id_prog(program_id), linked(false) { }

    GLSLProgram::~GLSLProgram() {
        if (id_prog) {
            glDeleteProgram(id_prog);
        }
    }

    bool GLSLProgram::link() {
        if (linked) {
            return true;
        }
        if (id_prog <= 0) {
            return false;
        }
        ///Program is not already linked, and is linkable
        glLinkProgram(id_prog);

        ///Gotta verify the link went Okay
        int status = 0;
        glGetProgramiv(id_prog, GL_LINK_STATUS, &status);
        if (GL_FALSE == status) {
            ///It failed, we need logs

            ///Get the length of the log
            int length = 0;
            glGetProgramiv(id_prog, GL_INFO_LOG_LENGTH, &length);

            if (length > 0) {
                ///The log has a non zero size
                // So allocate space for the log temporarily
                char* c_log = new char[length];
                int written = 0;
                glGetProgramInfoLog(id_prog, length, &written, c_log);
                CELog::error("An error happened trying to link. details: {}", c_log);
                delete[] c_log;
            }
            return false;
        }
        /// It didn't fail
        linked = true;
        return true;
    }

    void GLSLProgram::use() {
        assert(link());
        glUseProgram(id_prog);
    }

    void GLSLProgram::bind_attrib_location(GLuint location, const char* name) const {
        glBindAttribLocation(id_prog, location, name);
    }

    void GLSLProgram::bind_frag_data_location(GLuint location, const char* name) const {
        glBindFragDataLocation(id_prog, location, name);
    }

    void GLSLProgram::print_active_uniforms() const {

        GLint nUniforms, size, maxLen;
        GLsizei written;
        GLenum type;

        glGetProgramiv(id_prog, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxLen);
        glGetProgramiv(id_prog, GL_ACTIVE_UNIFORMS, &nUniforms);

        // todo: replace malloc/free
        const auto name = static_cast<GLchar *>(malloc(maxLen));

        std::cout<<" Location | Name\n";
        std::cout<<"------------------------------------------------\n";
        for (int i = 0; i < nUniforms; ++i) {
            glGetActiveUniform(id_prog, i, maxLen, &written, &size, &type, name);
            const GLint location = glGetUniformLocation(id_prog, name);
            std::cout<<location<<" | "<<name<<"\n";
        }

        free(name);
    }

    void GLSLProgram::print_active_attribs() const {

        GLint written, size, maxLength, nAttribs;
        GLenum type;

        glGetProgramiv(id_prog, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxLength);
        glGetProgramiv(id_prog, GL_ACTIVE_ATTRIBUTES, &nAttribs);

        const auto name = static_cast<GLchar *>(malloc(maxLength));

        std::cout<<" Index | Name\n";
        std::cout<<"------------------------------------------------\n";
        for (int i = 0; i < nAttribs; i++) {
            glGetActiveAttrib(id_prog, i, maxLength, &written, &size, &type, name);
            const GLint location = glGetAttribLocation(id_prog, name);
            std::cout<<location<<" | "<<name<<"\n";
        }

        free(name);
    }

    int GLSLProgram::get_uniform_location(const char* name) {
        int result = -1;
        if (linked) {
            if (locations.contains(name)) {
                return locations[name];
            }
            result = glGetUniformLocation(id_prog, name);
            if (result != -1) locations[name] = result;
        }
        return result;
    }

    int GLSLProgram::get_attribute_location(const char* name) {
        int result = -1;
        if (linked) {
            if (locations.contains(name)) {
                return locations[name];
            }
            result = glGetAttribLocation(id_prog, name);
            if (result != -1) locations[name] = result;
        }
        return result;
    }
}

