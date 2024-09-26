
#include <assets/primitives/glslprogram.h>
#include <iostream>
#include <fstream>
#include <filesystem>

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

bool GLSLProgram::compileShaderFromFile(const char* fileName, GLSLShader::GLSLShaderType type) {
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

    return compileShaderFromString(code.str(), type);
}

bool GLSLProgram::compileShaderFromString(const std::string &source, GLSLShader::GLSLShaderType type) {
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
    if (GL_FALSE == result) {
        // Compile failed, store log and return false
        int length = 0;
        log_info = "";
        glGetShaderiv(shaderHandle, GL_INFO_LOG_LENGTH, &length);
        if (length > 0) {
            char* c_log = new char[length];
            int written = 0;
            glGetShaderInfoLog(shaderHandle, length, &written, c_log);
            log_info = c_log;
            delete[] c_log;
        }
        assert(false);
        return false;
    } else {
        // Compile succeeded, attach shader and return true
        glAttachShader(id_prog, shaderHandle);
        return true;
    }
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

int GLSLProgram::getHandle() {
    return id_prog;
}

bool GLSLProgram::isLinked() {
    return linked;
}

void GLSLProgram::bindAttribLocation(GLuint location, const char* name) {
    glBindAttribLocation(id_prog, location, name);
}

void GLSLProgram::bindFragDataLocation(GLuint location, const char* name) {
    glBindFragDataLocation(id_prog, location, name);
}

void GLSLProgram::printActiveUniforms() {

    GLint nUniforms, size, location, maxLen;
    GLchar* name;
    GLsizei written;
    GLenum type;

    glGetProgramiv(id_prog, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxLen);
    glGetProgramiv(id_prog, GL_ACTIVE_UNIFORMS, &nUniforms);

    name = (GLchar*)malloc(maxLen);

    std::cout<<" Location | Name\n";
    std::cout<<"------------------------------------------------\n";
    for (int i = 0; i < nUniforms; ++i) {
        glGetActiveUniform(id_prog, i, maxLen, &written, &size, &type, name);
        location = glGetUniformLocation(id_prog, name);
        std::cout<<location<<" | "<<name<<"\n";
    }

    free(name);
}

void GLSLProgram::printActiveAttribs() {

    GLint written, size, location, maxLength, nAttribs;
    GLenum type;
    GLchar* name;

    glGetProgramiv(id_prog, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxLength);
    glGetProgramiv(id_prog, GL_ACTIVE_ATTRIBUTES, &nAttribs);

    name = (GLchar*)malloc(maxLength);

    std::cout<<" Index | Name\n";
    std::cout<<"------------------------------------------------\n";
    for (int i = 0; i < nAttribs; i++) {
        glGetActiveAttrib(id_prog, i, maxLength, &written, &size, &type, name);
        location = glGetAttribLocation(id_prog, name);
        std::cout<<location<<" | "<<name<<"\n";
    }

    free(name);
}

int GLSLProgram::getUniformLocation(const char* name) {
    //return glGetUniformLocation(handle, name);
    return GetUniform(name);
}

int GLSLProgram::GetUniform(const char* name) {
    int result = -1;

    if (linked) {
        UMapIter = UniformMap.find(name);
        if (UMapIter == UniformMap.end()) {
            result = glGetUniformLocation(id_prog, name);

            if (result != -1) UniformMap[name] = result;
        } else {
            result = UMapIter->second;
        }
    }

    return result;
}

int GLSLProgram::GetAttribute(const char* name) {
    int result = -1;

    if (linked) {
        UMapIter = UniformMap.find(name);
        if (UMapIter == UniformMap.end()) {
            result = glGetAttribLocation(id_prog, name);

            if (result != -1) UniformMap[name] = result;
        } else {
            result = UMapIter->second;
        }
    }

    return result;
}


