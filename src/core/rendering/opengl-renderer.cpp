#include <core/rendering/opengl-renderer.h>

#include <core.h>
#include <enums.h>
#include <assets/primitives/glslprogram.h>
#include <internals/exceptions.h>

#include <algorithm>
#include <fstream>
#include <format>
#include <sstream>
#include <vector>

using CE::Enum::ShaderTypes;
CE::RenderAPIs::program_id compile_src(const std::string& source, ShaderTypes type);

namespace CE::RenderAPIs {

    void OpenGLRenderer::initialize_glfw() {
        std::call_once(glfw_flag, [this]() {
            if (!glfwInit()) {
                throw Exceptions::runtime_exception(CE_HERE, "Failed to initialize GLFW.");
            }
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
            glfwWindowHint(GLFW_SAMPLES, 8);
            display = std::make_unique<DisplaySystem>();
            const auto pm = display->primary_monitor;
            float sw, sh;
            glfwGetMonitorContentScale(pm.glfw_monitor, &sw, &sh);
            sw = std::max(sw, 1.0f);
            sh = std::max(sh, 1.0f);
            const uint16_t width = std::lround(pm.width / sw);
            const uint16_t height = std::lround(pm.height / sh);
            display->create_window(pm, Enum::window_mode::NORMAL, width, height)->activate();
        });
    }

    void OpenGLRenderer::initialize_glad() {
        std::call_once(glad_flag, []() {
            if (!gladLoadGL(glfwGetProcAddress)) {
                throw Exceptions::runtime_exception(CE_HERE, "Failed to initialize OpenGL context");
            }
        });
    }

    void OpenGLRenderer::initialize_libraries() {
        initialize_glfw();
        initialize_glad();
        lib_init = true;
    }

    void OpenGLRenderer::initialize_rendering_context() {
        if (!lib_init) {
            initialize_libraries();
        }
        if (!display->active || !display->active->glfw_window) {
            throw Exceptions::runtime_exception(CE_HERE, "Cannot initialize rendering without an active window");
        }
        int framebuffer_width = 0;
        int framebuffer_height = 0;
        glfwGetFramebufferSize(display->active->glfw_window, &framebuffer_width, &framebuffer_height);
        glViewport(0, 0, framebuffer_width, framebuffer_height);
        /// Here we query how much sampling is possible and set that to be used if possible
        GLint samples = 0;
        glGetIntegerv(GL_SAMPLES, &samples);
        if (samples > 0) {
            glEnable(GL_MULTISAMPLE);
        }

        // where are we? (part 1)
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        // what are we doing? (part 2)
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // it's dark here. (part 3)
        glClearColor(0.f, 0.f, 0.f, 0.f); // white or black, dunno
        glfwSwapInterval(1);
    }

    void OpenGLRenderer::deinitialize() {
        glfwMakeContextCurrent(nullptr);
    }

    void OpenGLRenderer::clear() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRenderer::swap_buffer() {
        glfwSwapBuffers(display->active->glfw_window);
    }

    void OpenGLRenderer::draw() {
        // todo: figure it out
    }

    program_id OpenGLRenderer::compile_shader(fs::path file) {
        if (!fs::exists(file)) {
            CELog::critical("GLSLProgram: File does not exist. {}", file.c_str());
            throw Exceptions::failed_operation(CE_HERE, "No such file exists");
        }

        std::ifstream inFile(file, std::ios::in);
        if (!inFile) {
            assert(false);
            return false;
        }

        std::ostringstream code;
        while (inFile.good()) {
            int c = inFile.get();
            if (!inFile.eof())
                code << static_cast<char>(c);
        }
        inFile.close();
        return compile_src(code.str(), Enum::get_shader_type(file.extension().string()));
    }

    program_id OpenGLRenderer::compile_program(const std::vector<fs::path>& files) {
        if (files.size() < 2) {
            throw Exceptions::invalid_args(CE_HERE, "A shader program needs vertex and fragment stages");
        }
        const GLuint program = glCreateProgram();
        if (!program) {
            throw Exceptions::runtime_exception(CE_HERE, "Failed to create an OpenGL program");
        }
        std::vector<GLuint> compiled;
        try {
            for (const auto& file : files) {
                std::ifstream input(file);
                if (!input) {
                    throw Exceptions::runtime_exception(CE_HERE, "Unable to open shader: " + file.string());
                }
                std::ostringstream buffer;
                buffer << input.rdbuf();
                const auto source = buffer.str();
                const auto type = Enum::get_shader_type(file.extension().string());
                GLenum gl_type = GL_FRAGMENT_SHADER;
                switch (type) {
                case ShaderTypes::VERTEX:
                    gl_type = GL_VERTEX_SHADER;
                    break;
                case ShaderTypes::FRAGMENT:
                    gl_type = GL_FRAGMENT_SHADER;
                    break;
                case ShaderTypes::GEOMETRY:
                    gl_type = GL_GEOMETRY_SHADER;
                    break;
                case ShaderTypes::TESS_CONTROL:
                    gl_type = GL_TESS_CONTROL_SHADER;
                    break;
                case ShaderTypes::TESS_EVALUATION:
                    gl_type = GL_TESS_EVALUATION_SHADER;
                    break;
                }
                const GLuint stage = glCreateShader(gl_type);
                if (!stage) {
                    throw Exceptions::runtime_exception(CE_HERE, "Failed to create shader stage: " + file.string());
                }
                compiled.push_back(stage);
                const char* data = source.c_str();
                glShaderSource(stage, 1, &data, nullptr);
                glCompileShader(stage);
                GLint success = GL_FALSE;
                glGetShaderiv(stage, GL_COMPILE_STATUS, &success);
                if (success != GL_TRUE) {
                    GLint size = 0;
                    glGetShaderiv(stage, GL_INFO_LOG_LENGTH, &size);
                    std::string log(static_cast<std::size_t>(std::max(size, 1)), '\0');
                    glGetShaderInfoLog(stage, size, nullptr, log.data());
                    throw Exceptions::runtime_exception(CE_HERE,
                                                        "Shader compile failed: " + file.string() + "\n" + log);
                }
                glAttachShader(program, stage);
            }
            glLinkProgram(program);
            GLint success = GL_FALSE;
            glGetProgramiv(program, GL_LINK_STATUS, &success);
            if (success != GL_TRUE) {
                GLint size = 0;
                glGetProgramiv(program, GL_INFO_LOG_LENGTH, &size);
                std::string log(static_cast<std::size_t>(std::max(size, 1)), '\0');
                glGetProgramInfoLog(program, size, nullptr, log.data());
                throw Exceptions::runtime_exception(CE_HERE, "Shader program link failed:\n" + log);
            }
        }
        catch (...) {
            for (const GLuint stage : compiled)
                glDeleteShader(stage);
            glDeleteProgram(program);
            throw;
        }
        for (const GLuint stage : compiled) {
            glDetachShader(program, stage);
            glDeleteShader(stage);
        }
        return program;
    }
}

CE::RenderAPIs::program_id compile_src(const std::string& source, ShaderTypes type) {
    const int id_prog = glCreateProgram();
    if (id_prog == 0) {
        throw CE::Exceptions::failed_operation(CE_HERE, "Unable to create GLSL program id.");
    }

    GLuint shaderHandle = 0;

    switch (type) {
    case ShaderTypes::VERTEX:
        shaderHandle = glCreateShader(GL_VERTEX_SHADER);
        break;
    case ShaderTypes::FRAGMENT:
        shaderHandle = glCreateShader(GL_FRAGMENT_SHADER);
        break;
    case ShaderTypes::GEOMETRY:
        shaderHandle = glCreateShader(GL_GEOMETRY_SHADER);
        break;
    case ShaderTypes::TESS_CONTROL:
        shaderHandle = glCreateShader(GL_TESS_CONTROL_SHADER);
        break;
    case ShaderTypes::TESS_EVALUATION:
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
        glGetShaderiv(shaderHandle, GL_INFO_LOG_LENGTH, &length);
        std::string log;
        if (length > 0) {
            const auto c_log = new char[length];
            int written = 0;
            glGetShaderInfoLog(shaderHandle, length, &written, c_log);
            log = c_log;
            delete[] c_log;
        }
        throw CE::Exceptions::runtime_exception(CE_HERE,
                                                std::format("Unable to compile glsl program.\n{}", log).c_str());
    }
    // Compile succeeded, attach shader and return id
    glAttachShader(id_prog, shaderHandle);
    return id_prog;
}
