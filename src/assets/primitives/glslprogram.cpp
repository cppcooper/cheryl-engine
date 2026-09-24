
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
    GLSLProgram::GLSLProgram(int program_id) : id_prog(program_id), linked(false) {
        GLint status = GL_FALSE;
        glGetProgramiv(id_prog, GL_LINK_STATUS, &status);
        linked = status == GL_TRUE;
    }

    GLSLProgram::~GLSLProgram() {
        // TODO: Route deletion through a retained GL context/release queue. This
        // call requires a current context; a shader outliving it is still unsafe.
        if (id_prog) {
            glDeleteProgram(id_prog);
        }
    }

    bool GLSLProgram::link() {
        // A provider may supply either a linked program or a stage-bearing program awaiting
        // the link step; avoid relinking an executable that is already ready to use.
        if (linked) {
            return true;
        }
        if (id_prog <= 0) {
            return false;
        }
        // Link attached stages, then query the program status before permitting use().
        glLinkProgram(id_prog);

        int status = 0;
        glGetProgramiv(id_prog, GL_LINK_STATUS, &status);
        if (GL_FALSE == status) {
            // Preserve the driver's diagnostic when a stage interface fails to link.
            int length = 0;
            glGetProgramiv(id_prog, GL_INFO_LOG_LENGTH, &length);

            if (length > 0) {
                char* c_log = new char[length];
                int written = 0;
                glGetProgramInfoLog(id_prog, length, &written, c_log);
                CELog::error("An error happened trying to link. details: {}", c_log);
                delete[] c_log;
            }
            return false;
        }
        linked = true;
        return true;
    }

    void GLSLProgram::use() {
        if (!link()) {
            throw Exceptions::runtime_exception(CE_HERE, "Cannot use an unlinked shader program");
        }
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
        // Query OpenGL once after linking, caching valid locations for repeated draw calls.
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
