#include <backends/opengl/resource-provider.h>

#include <backends/opengl/glslprogram.h>
#include <backends/opengl/renderer.h>
#include <backends/opengl/texture.h>
#include <backends/opengl/vertex-array-object.h>
#include <internals/exceptions.h>

#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <utility>

namespace CE::Assets {
    namespace {
        GLenum stage_type(const std::filesystem::path& file) {
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

        std::string shader_log(const GLuint shader) {
            GLint length = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
            if (length <= 1)
                return {};
            std::string log(static_cast<std::size_t>(length), '\0');
            glGetShaderInfoLog(shader, length, nullptr, log.data());
            log.resize(static_cast<std::size_t>(length - 1));
            return log;
        }

        std::string program_log(const GLuint program) {
            GLint length = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
            if (length <= 1)
                return {};
            std::string log(static_cast<std::size_t>(length), '\0');
            glGetProgramInfoLog(program, length, nullptr, log.data());
            log.resize(static_cast<std::size_t>(length - 1));
            return log;
        }
    }

    std::shared_ptr<Image> OpenGLResourceProvider::create_image(const DecodedImage& image) {
        const auto width = static_cast<std::size_t>(image.size.width);
        const auto height = static_cast<std::size_t>(image.size.height);
        if (width == 0 || height == 0 || width > std::numeric_limits<int>::max() || height > std::numeric_limits<int>::max() ||
            width > std::numeric_limits<std::size_t>::max() / 4 || height > std::numeric_limits<std::size_t>::max() / (width * 4) ||
            image.rgba.size() != width * height * 4)
            throw Exceptions::invalid_args(CE_HERE, "RGBA pixels do not match the requested dimensions");
        return std::make_shared<Texture>(renderer_.resources(), image.rgba.data(), static_cast<int>(width), static_cast<int>(height),
            GL_TEXTURE0, true, false, GL_CLAMP_TO_EDGE, GL_RGBA);
    }

    std::shared_ptr<Image> OpenGLResourceProvider::create_font_atlas(const std::span<const unsigned char> alpha, const PixelSize size) {
        if (size.width == 0 || size.height == 0 || size.width > std::numeric_limits<int>::max() ||
            size.height > std::numeric_limits<int>::max() || size.height > std::numeric_limits<std::size_t>::max() / size.width ||
            alpha.size() != static_cast<std::size_t>(size.width) * size.height) {
            throw Exceptions::invalid_args(CE_HERE, "Font atlas pixels do not match the requested dimensions");
        }
        return std::make_shared<Texture>(renderer_.resources(), alpha.data(), static_cast<int>(size.width), static_cast<int>(size.height),
            GL_TEXTURE0, false, false, GL_CLAMP_TO_EDGE, GL_RED);
    }

    std::shared_ptr<Geometry2D> OpenGLResourceProvider::upload_geometry(
        const std::span<const Vertex2D> vertices,
        const PrimitiveTopology topology
    ) {
        return std::make_shared<VAO>(renderer_.resources(), vertices, topology);
    }

    std::shared_ptr<Shader> OpenGLResourceProvider::link_program(const std::vector<std::filesystem::path>& stages) {
        auto lifetime = renderer_.resources();
        if (stages.empty())
            throw Exceptions::invalid_args(CE_HERE, "A shader program needs at least one stage");

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
        if (!program.id)
            throw Exceptions::failed_operation(CE_HERE, "Could not create an OpenGL program");

        for (const auto& file : stages) {
            const auto kind = stage_type(file);
            std::ifstream input(file, std::ios::binary);
            if (!input)
                throw Exceptions::runtime_exception(CE_HERE, "Could not open shader stage: " + file.string());
            const std::string source(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
            if (input.bad() || source.size() > static_cast<std::size_t>(std::numeric_limits<GLint>::max()))
                throw Exceptions::runtime_exception(CE_HERE, "Could not read shader stage: " + file.string());

            ShaderGuard shader{lifetime, glCreateShader(kind)};
            if (!shader.id)
                throw Exceptions::failed_operation(CE_HERE, "Could not create shader stage: " + file.string());
            const GLchar* bytes = source.c_str();
            const auto length = static_cast<GLint>(source.size());
            glShaderSource(shader.id, 1, &bytes, &length);
            glCompileShader(shader.id);
            GLint compiled = GL_FALSE;
            glGetShaderiv(shader.id, GL_COMPILE_STATUS, &compiled);
            if (compiled != GL_TRUE)
                throw Exceptions::runtime_exception(CE_HERE,
                    "Shader stage failed to compile (" + file.string() + "): " + shader_log(shader.id));
            glAttachShader(program.id, shader.id);
            // The program retains the stage after glDeleteShader until linking/deletion.
        }

        glLinkProgram(program.id);
        GLint linked = GL_FALSE;
        glGetProgramiv(program.id, GL_LINK_STATUS, &linked);
        if (linked != GL_TRUE)
            throw Exceptions::runtime_exception(CE_HERE, "Shader program failed to link: " + program_log(program.id));

        // Adopt once before any later logical-program allocations can fail.
        // From here on, only the tracked handle owns retirement of this ID.
        RenderAPIs::OpenGLHandle tracked(lifetime, RenderAPIs::GLResourceKind::Program, program.id);
        program.id = 0;
        return std::make_shared<GLSLProgram>(std::move(tracked));
    }
}
