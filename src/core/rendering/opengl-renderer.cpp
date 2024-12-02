#include <core.h>
#include <enums.h>
#include <assets/primitives/glslprogram.h>
#include <sstream>
#include <fstream>

using CE::Enum::ShaderTypes;
CE::RenderAPIs::program_id compile_src(const std::string &source, ShaderTypes type);

namespace CE::RenderAPIs{
    void OpenGLRenderer::initialize_libraries() {
        std::once_flag flag;
        std::call_once(flag, []() {
            gladLoadGL(glfwGetProcAddress);
            glfwInit();
        });
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        /// Here we query how much sampling is possible and set that to be used if possible
        GLint samples = 8;
        glGetIntegerv(GL_SAMPLES, &samples);
        if (samples) {
            glEnable(GL_MULTISAMPLE);
        }
        glfwWindowHint(GLFW_SAMPLES, samples);
    }

    void OpenGLRenderer::initialize_rendering_context() {
        const auto pm = display.primary_monitor;
        display.create_window(pm, Enum::window_mode::FULLSCREEN, pm.width, pm.height)->activate();
        /// If creating the window failed we need to terminate
        if (!display.active || !display.active->glfw_window) {
            glfwTerminate();
            return;
        }
        SubSystems::EventSystem::get().register_listener("window-resized",[this](std::any payload) {
            if (!payload.has_value()) {
                throw Exceptions::failed_operation(CE_HERE,"An event (\"window-resized\") was dispatched without a payload.");
            }
            try {
                const auto window = std::get<0>(std::any_cast<std::tuple<Window*,float,float>>(payload));
                // todo: update camera calculations
                //calculate_projection();
            } catch (const std::bad_any_cast& e) {
                CELog::error("Event payload was illformed.\n{}", e.what());
            }
        });

        // where are we? (part 1)
        glEnable( GL_CULL_FACE );
        glCullFace( GL_BACK );
        glFrontFace( GL_CCW );

        // what are we doing? (part 2)
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

        // it's dark here. (part 3)
        glClearColor(0.f,0.f,0.f,0.f); //white or black, dunno
        glfwSwapInterval(1);
    }

    void OpenGLRenderer::clear() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRenderer::swap_buffer() {
        glfwSwapBuffers(display.active->glfw_window);
    }

    void OpenGLRenderer::draw() {
        // todo: figure it out
    }

    program_id OpenGLRenderer::compile_shader(fs::path file) {
        if (!fs::exists(file)) {
            CELog::critical("GLSLProgram: File does not exist. {}", file);
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
            if (!inFile.eof()) code << static_cast<char>(c);
        }
        inFile.close();
        return compile_src(code.str(), Enum::get_shader_type(file.extension().string()));
    }
}

CE::RenderAPIs::program_id compile_src(const std::string &source, ShaderTypes type) {
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
            std::format("Unable to compile glsl program.\n{}",log).c_str());
    }
    // Compile succeeded, attach shader and return id
    glAttachShader(id_prog, shaderHandle);
    return id_prog;
}