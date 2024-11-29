
#include <assets/primitives/glslprogram.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <internals/exceptions.h>

using std::ifstream;
using std::ios;

#include <sstream>

using std::ostringstream;
namespace fs = std::filesystem;

GLSLProgram::GLSLProgram() : id_prog(0), linked(false) { }

GLSLProgram::~GLSLProgram() {
    if (id_prog) {
        glDeleteProgram(id_prog);
    }
}

bool GLSLProgram::compile_file(const char* fileName, GLSLShader::GLSLShaderType type) {
    if (!fs::exists(fileName)) {
        log_info = "File not found.";
        return false;
    }

    if (id_prog <= 0) {
        id_prog = glCreateProgram();
        if (id_prog == 0) {
            log_info = "Unable to create shader program.";
            return false;
        }
    }

    ifstream inFile(fileName, ios::in);
    if (!inFile) {
        assert(false);
        return false;
    }

    ostringstream code;
    while (inFile.good()) {
        int c = inFile.get();
        if (!inFile.eof()) code<<(char)c;
    }
    inFile.close();

    return compile_src(code.str(), type);
}

bool GLSLProgram::compile_src(const std::string &source, GLSLShader::GLSLShaderType type) {
    if (id_prog <= 0) {
        id_prog = glCreateProgram();
        if (id_prog == 0) {
            log_info = "Unable to create shader program.";
            assert(false);
        }
    }

    GLuint shaderHandle = 0;

    switch (type) {
        case GLSLShader::VERTEX:
            shaderHandle = glCreateShader(GL_VERTEX_SHADER);
            break;
        case GLSLShader::FRAGMENT:
            shaderHandle = glCreateShader(GL_FRAGMENT_SHADER);
            break;
        case GLSLShader::GEOMETRY:
            shaderHandle = glCreateShader(GL_GEOMETRY_SHADER);
            break;
        case GLSLShader::TESS_CONTROL:
            shaderHandle = glCreateShader(GL_TESS_CONTROL_SHADER);
            break;
        case GLSLShader::TESS_EVALUATION:
            shaderHandle = glCreateShader(GL_TESS_EVALUATION_SHADER);
            break;
        default:
            return false;
    }

    const char* c_code = source.c_str();
    glShaderSource(shaderHandle, 1, &c_code, NULL);

    // Compile the shader
    glCompileShader(shaderHandle);

    // Check for errors
    int result;
    glGetShaderiv(shaderHandle, GL_COMPILE_STATUS, &result);
    // Did the compile fail, store log and return false
    if (result == GL_FALSE) {
        int length = 0;
        log_info = "";
        glGetShaderiv(shaderHandle, GL_INFO_LOG_LENGTH, &length);
        if (length > 0) {
            const auto c_log = new char[length];
            int written = 0;
            glGetShaderInfoLog(shaderHandle, length, &written, c_log);
            log_info = c_log;
            delete[] c_log;
        }
        // todo: replace assert with thrown exception
        throw CE::Exceptions::runtime_exception(CE_HERE, "Unable to compile glsl program.");
    }
    // Compile succeeded, attach shader and return true
    glAttachShader(id_prog, shaderHandle);
    // todo: link object to the variables it depends on
    return true;
}

bool GLSLProgram::link() {
    if (linked) {
        return true;
    } else if (id_prog <= 0) {
        return false;
    } else {
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
                log_info = c_log;
                delete[] c_log;
            }
            return false;
        } else {
            /// It didn't fail
            linked = true;
            return true;
        }
    }
}

void GLSLProgram::use() {
    assert(link());
    glUseProgram(id_prog);
}

std::string GLSLProgram::log() {
    return log_info;
}

int GLSLProgram::get_handle() const {
    return id_prog;
}

bool GLSLProgram::is_linked() const {
    return linked;
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


