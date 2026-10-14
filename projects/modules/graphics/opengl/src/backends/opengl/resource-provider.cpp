#include <backends/opengl/resource-provider.h>

#include "program-builder.h"

#include <backends/opengl/glslprogram.h>
#include <backends/opengl/renderer.h>
#include <backends/opengl/sampler.h>
#include <backends/opengl/texture.h>
#include <backends/opengl/vertex-array-object.h>
#include <internals/exceptions.h>

#include <limits>
#include <memory>
#include <utility>

namespace CE::Rendering::OpenGL {
    std::shared_ptr<CE::Rendering::Resources::Image> ResourceProvider::create_image(const CE::Assets::Images::Decoded& image) {
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

    std::shared_ptr<CE::Rendering::Resources::Image>
    ResourceProvider::create_font_atlas(const std::span<const unsigned char> alpha, const CE::Rendering::Geometry::PixelSize size) {
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

    std::shared_ptr<const CE::Rendering::Sampling::Sampler>
    ResourceProvider::create_sampler(const CE::Rendering::Sampling::Options& options) {
        const auto lifetime = renderer_.resources();
        lifetime->require_current();
        CE::Rendering::Sampling::validate(options);
        std::erase_if(samplers_, [](const auto& entry) { return entry.second.expired(); });
        if (const auto found = samplers_.find(options); found != samplers_.end()) {
            if (auto sampler = found->second.lock())
                return sampler;
        }
        auto sampler = std::make_shared<Sampler>(lifetime, options);
        samplers_.insert_or_assign(options, sampler);
        return sampler;
    }

    std::shared_ptr<CE::Rendering::Geometry::Buffer2D> ResourceProvider::upload_geometry(
        const std::span<const CE::Rendering::Geometry::Vertex2D> vertices,
        const CE::Rendering::Geometry::Topology topology
    ) {
        return std::make_shared<VertexArray>(renderer_.resources(), vertices, topology);
    }

    std::shared_ptr<CE::Rendering::Resources::Program> ResourceProvider::link_program(const std::vector<std::filesystem::path>& stages) {
        return Detail::link_program(renderer_.resources(), stages);
    }

    std::shared_ptr<CE::Rendering::Geometry::Buffer2D> ResourceProvider::upload_geometry(
        const std::span<const CE::Rendering::Geometry::Vertex2DColor> vertices,
        const CE::Rendering::Geometry::Topology topology
    ) {
        return std::make_shared<VertexArray>(renderer_.resources(), vertices, topology);
    }

    std::shared_ptr<const Pipeline>
    ResourceProvider::build_pipeline(CE::Rendering::Pipelines::Definition definition, const PipelineBindings& bindings) {
        CE::Rendering::Pipelines::validate(definition);
        auto program = std::dynamic_pointer_cast<Program>(link_program(definition.program_sources));
        return std::make_shared<Pipeline>(std::move(definition), std::move(program), bindings);
    }

    std::shared_ptr<const CE::Rendering::Resources::Material>
    ResourceProvider::build_material(CE::Rendering::Materials::Definition definition) {
        const auto* pipeline = dynamic_cast<const Pipeline*>(definition.pipeline.get());
        if (!pipeline || pipeline->resource_domain() != renderer_.resources().get())
            throw Exceptions::invalid_args(CE_HERE, "Material requires a pipeline from this OpenGL provider's domain");
        for (const auto& [key, sampling] : definition.sampling) {
            static_cast<void>(key);
            const auto* sampler = dynamic_cast<const Sampler*>(sampling.get());
            if (!sampler || sampler->resource_domain() != renderer_.resources().get())
                throw Exceptions::invalid_args(CE_HERE, "Material sampling requires this OpenGL provider's domain");
            sampler->require_binding(0);
        }
        auto material = std::make_shared<CE::Rendering::Resources::Material>(std::move(definition));
        pipeline->validate_resources(material->definition().defaults);
        return material;
    }
}
