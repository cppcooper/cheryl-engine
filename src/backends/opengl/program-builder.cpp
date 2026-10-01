#include "program-builder.h"
#include "resource-lifetime-internal.h"
#include "upload-check.h"

#include <backends/opengl/glslprogram.h>
#include <internals/exceptions.h>

#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <utility>

namespace CE::Assets::ProgramDetail {
    namespace {
        GLenum stage_type(
            const std::filesystem::path& file
        ) {
            const auto extension = file.extension();
            if (extension == ".vert")
                return GL_VERTEX_SHADER;
            if (extension == ".frag")
                return GL_FRAGMENT_SHADER;
            if (extension == ".geo")
                return GL_GEOMETRY_SHADER;
            if (extension == ".tesc")
                return GL_TESS_CONTROL_SHADER;
            if (extension == ".tese")
                return GL_TESS_EVALUATION_SHADER;
            throw Exceptions::invalid_args(CE_HERE, "Unknown shader stage: " + file.string());
        }

        std::string shader_log(
            const GLuint shader
        ) {
            GLint length = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
            RenderAPIs::require_no_gl_error("Could not query shader log length");
            if (length <= 1)
                return {};
            std::string log(static_cast<std::size_t>(length), '\0');
            glGetShaderInfoLog(shader, length, nullptr, log.data());
            RenderAPIs::require_no_gl_error("Could not read shader log");
            log.resize(static_cast<std::size_t>(length - 1));
            return log;
        }

        std::string program_log(
            const GLuint program
        ) {
            GLint length = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
            RenderAPIs::require_no_gl_error("Could not query program log length");
            if (length <= 1)
                return {};
            std::string log(static_cast<std::size_t>(length), '\0');
            glGetProgramInfoLog(program, length, nullptr, log.data());
            RenderAPIs::require_no_gl_error("Could not read program log");
            log.resize(static_cast<std::size_t>(length - 1));
            return log;
        }
    }

    std::shared_ptr<GLSLProgram> link_program(
        std::shared_ptr<RenderAPIs::OpenGLResourceLifetime> lifetime,
        const std::vector<std::filesystem::path>& stages,
        std::shared_ptr<std::pmr::memory_resource> logical_memory
    ) {
        if (stages.empty())
            throw Exceptions::invalid_args(CE_HERE, "A shader program needs at least one stage");

        if (!lifetime)
            throw Exceptions::invalid_args(CE_HERE, "Shader construction requires an OpenGL resource lifetime");
        lifetime->require_current();
        RenderAPIs::require_no_gl_error("OpenGL error before program construction");

        struct ProgramGuard {
            std::shared_ptr<RenderAPIs::OpenGLResourceLifetime> lifetime;
            GLuint id;

            ~ProgramGuard() { lifetime->discard_untracked(RenderAPIs::GLResourceKind::Program, id); }
        } program{lifetime, glCreateProgram()};
        struct ShaderGuard {
            std::shared_ptr<RenderAPIs::OpenGLResourceLifetime> lifetime;
            GLuint id;

            ~ShaderGuard() { lifetime->discard_untracked(RenderAPIs::GLResourceKind::ShaderStage, id); }
        };
        RenderAPIs::require_no_gl_error("Could not create an OpenGL program");
        if (!program.id)
            throw Exceptions::failed_operation(CE_HERE, "Could not create an OpenGL program");

        std::vector<GLuint> attached_stages;
        attached_stages.reserve(stages.size());
        for (const auto& file : stages) {
            const auto kind = stage_type(file);
            std::ifstream input(file, std::ios::binary);
            if (!input)
                throw Exceptions::runtime_exception(CE_HERE, "Could not open shader stage: " + file.string());
            const std::string source(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
            if (input.bad() || source.size() > static_cast<std::size_t>(std::numeric_limits<GLint>::max()))
                throw Exceptions::runtime_exception(CE_HERE, "Could not read shader stage: " + file.string());

            ShaderGuard shader{lifetime, glCreateShader(kind)};
            RenderAPIs::require_no_gl_error("Could not create an OpenGL shader stage");
            if (!shader.id)
                throw Exceptions::failed_operation(CE_HERE, "Could not create shader stage: " + file.string());
            const GLchar* bytes = source.c_str();
            const auto length = static_cast<GLint>(source.size());
            glShaderSource(shader.id, 1, &bytes, &length);
            RenderAPIs::require_no_gl_error("Could not upload shader source");
            glCompileShader(shader.id);
            RenderAPIs::require_no_gl_error("Could not compile shader stage");
            GLint compiled = GL_FALSE;
            glGetShaderiv(shader.id, GL_COMPILE_STATUS, &compiled);
            RenderAPIs::require_no_gl_error("Could not query shader compile status");
            if (compiled != GL_TRUE)
                throw Exceptions::runtime_exception(CE_HERE,
                    "Shader stage failed to compile (" + file.string() + "): " + shader_log(shader.id));
            glAttachShader(program.id, shader.id);
            RenderAPIs::require_no_gl_error("Could not attach shader stage");
            attached_stages.push_back(shader.id);
            // Deletion is deferred while attached; detach after linking below.
        }

        glLinkProgram(program.id);
        RenderAPIs::require_no_gl_error("Could not link shader program");
        for (const auto shader : attached_stages) {
            glDetachShader(program.id, shader);
            RenderAPIs::require_no_gl_error("Could not detach shader stage");
        }
        GLint linked = GL_FALSE;
        glGetProgramiv(program.id, GL_LINK_STATUS, &linked);
        RenderAPIs::require_no_gl_error("Could not query program link status");
        if (linked != GL_TRUE)
            throw Exceptions::runtime_exception(CE_HERE, "Shader program failed to link: " + program_log(program.id));

        // Adopt once before any later logical-program allocations can fail.
        // From here on, only the tracked handle owns retirement of this ID.
        RenderAPIs::OpenGLHandle tracked(lifetime, RenderAPIs::GLResourceKind::Program, program.id);
        program.id = 0;
        if (logical_memory)
            return std::allocate_shared<GLSLProgram>(
                RenderAPIs::ResourceDetail::RetainedMemoryAllocator<GLSLProgram>{std::move(logical_memory)},
                std::move(tracked)
            );
        return std::make_shared<GLSLProgram>(std::move(tracked));
    }
}
