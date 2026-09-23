#include <core/resources/opengl-resource-provider.h>

#include <assets/primitives/glslprogram.h>
#include <assets/primitives/texture.h>
#include <assets/primitives/vertex-array-object.h>
#include <core/rendering/opengl-renderer.h>
#include <internals/exceptions.h>

#include <limits>
#include <memory>
#include <utility>

namespace CE::Assets {
    std::shared_ptr<Image> OpenGLResourceProvider::load_image(const std::filesystem::path& file) {
        return std::make_shared<Texture>(file.string().c_str(), GL_TEXTURE0, true, false, GL_CLAMP_TO_EDGE);
    }

    std::shared_ptr<Image> OpenGLResourceProvider::create_font_atlas(const std::span<const unsigned char> alpha,
                                                                      const PixelSize size) {
        if (size.width == 0 || size.height == 0 ||
            size.width > std::numeric_limits<int>::max() || size.height > std::numeric_limits<int>::max() ||
            alpha.size() != static_cast<std::size_t>(size.width) * size.height) {
            throw Exceptions::invalid_args(CE_HERE, "Font atlas pixels do not match the requested dimensions");
        }
        return std::make_shared<Texture>(alpha.data(), static_cast<int>(size.width), static_cast<int>(size.height),
                                         GL_TEXTURE0, false, false, GL_CLAMP_TO_EDGE, GL_RED);
    }

    std::shared_ptr<Geometry2D> OpenGLResourceProvider::upload_geometry(std::shared_ptr<Vertex2D> vertices,
                                                                          const std::uint32_t vertex_count) {
        if (!vertices || vertex_count == 0)
            throw Exceptions::invalid_args(CE_HERE, "Cannot upload empty 2D geometry");
        return std::make_shared<VAO>(std::move(vertices), vertex_count);
    }

    std::shared_ptr<Shader> OpenGLResourceProvider::compile_stage(const std::filesystem::path& file) {
        return std::make_shared<GLSLProgram>(static_cast<int>(renderer_.compile_shader(file)));
    }

    std::shared_ptr<Shader> OpenGLResourceProvider::link_program(
        const std::vector<std::filesystem::path>& stages) {
        return std::make_shared<GLSLProgram>(static_cast<int>(renderer_.compile_program(stages)));
    }
}
