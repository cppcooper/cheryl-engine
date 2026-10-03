#include <backends/opengl/resource-provider.h>

#include "program-builder.h"

#include <backends/opengl/glslprogram.h>
#include <backends/opengl/renderer.h>
#include <backends/opengl/texture.h>
#include <backends/opengl/vertex-array-object.h>
#include <internals/exceptions.h>

#include <limits>
#include <memory>
#include <utility>

namespace CE::Assets {
    std::shared_ptr<Image> OpenGLResourceProvider::create_image(const DecodedImage& image) {
        const auto width = static_cast<std::size_t>(image.size.width);
        const auto height = static_cast<std::size_t>(image.size.height);
        if (width == 0 || height == 0 || width > std::numeric_limits<int>::max() || height > std::numeric_limits<int>::max() ||
            width > std::numeric_limits<std::size_t>::max() / 4 || height > std::numeric_limits<std::size_t>::max() / (width * 4) ||
            image.rgba.size() != width * height * 4)
            throw Exceptions::invalid_args(CE_HERE, "RGBA pixels do not match the requested dimensions");
        return std::make_shared<Texture>(
            renderer_.resources(), image.rgba.data(), static_cast<int>(width), static_cast<int>(height), true, false, GL_CLAMP_TO_EDGE,
            GL_RGBA
        );
    }

    std::shared_ptr<Image> OpenGLResourceProvider::create_font_atlas(const std::span<const unsigned char> alpha, const PixelSize size) {
        if (size.width == 0 || size.height == 0 || size.width > std::numeric_limits<int>::max() ||
            size.height > std::numeric_limits<int>::max() || size.height > std::numeric_limits<std::size_t>::max() / size.width ||
            alpha.size() != static_cast<std::size_t>(size.width) * size.height) {
            throw Exceptions::invalid_args(CE_HERE, "Font atlas pixels do not match the requested dimensions");
        }
        return std::make_shared<Texture>(
            renderer_.resources(), alpha.data(), static_cast<int>(size.width), static_cast<int>(size.height), false, false,
            GL_CLAMP_TO_EDGE, GL_RED
        );
    }

    std::shared_ptr<Geometry2D>
    OpenGLResourceProvider::upload_geometry(const std::span<const Vertex2D> vertices, const PrimitiveTopology topology) {
        return std::make_shared<VAO>(renderer_.resources(), vertices, topology);
    }

    std::shared_ptr<Shader> OpenGLResourceProvider::link_program(const std::vector<std::filesystem::path>& stages) {
        return ProgramDetail::link_program(renderer_.resources(), stages);
    }

    std::shared_ptr<const GLSLPipeline>
    OpenGLResourceProvider::build_pipeline(PipelineDefinition definition, const GLSLPipelineBindings& bindings) {
        validate_pipeline_definition(definition);
        auto program = std::dynamic_pointer_cast<GLSLProgram>(link_program(definition.program_sources));
        return std::make_shared<GLSLPipeline>(std::move(definition), std::move(program), bindings);
    }

    std::shared_ptr<const Material> OpenGLResourceProvider::build_material(MaterialDefinition definition) {
        const auto* pipeline = dynamic_cast<const GLSLPipeline*>(definition.pipeline.get());
        if (!pipeline || pipeline->resource_domain() != renderer_.resources().get())
            throw Exceptions::invalid_args(CE_HERE, "Material requires a pipeline from this OpenGL provider's domain");
        auto material = std::make_shared<Material>(std::move(definition));
        pipeline->validate_resources(material->definition().defaults);
        return material;
    }
}
