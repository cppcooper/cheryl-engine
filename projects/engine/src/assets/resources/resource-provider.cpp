#include <assets/resources/resource-provider.h>

#include <core/resources/asset-management/font-mgr.h>
#include <core/resources/asset-management/material-mgr.h>
#include <core/resources/asset-management/shader-mgr.h>
#include <core/resources/asset-management/shader-asset-mgr.h>
#include <core/resources/asset-management/sprite-mgr.h>
#include <core/resources/asset-management/texture-mgr.h>
#include <core/resources/asset-management/tileset-mgr.h>
#include <internals/exceptions.h>

namespace CE::Rendering::Resources {
    std::shared_ptr<Image> Provider::load_image(const std::filesystem::path& file) {
        return create_image(CE::Assets::Images::decode(file));
    }

    std::shared_ptr<const CE::Rendering::Sampling::Sampler> Provider::create_sampler(const CE::Rendering::Sampling::Options& options) {
        CE::Rendering::Sampling::validate(options);
        throw Exceptions::failed_operation(CE_HERE, "Resource provider does not support image samplers");
    }

    std::shared_ptr<CE::Rendering::Geometry::Buffer2D>
    Provider::upload_geometry(std::span<const CE::Rendering::Geometry::Vertex2DColor>, CE::Rendering::Geometry::Topology) {
        throw Exceptions::failed_operation(CE_HERE, "Resource provider does not support colored 2D geometry");
    }

    std::shared_ptr<CE::Rendering::Geometry::Buffer2D> Provider::upload_geometry(
        std::shared_ptr<CE::Rendering::Geometry::Vertex2D> vertices,
        const std::uint32_t vertex_count,
        const CE::Rendering::Geometry::Topology topology
    ) {
        if (!vertices || vertex_count == 0)
            throw Exceptions::invalid_args(CE_HERE, "Cannot upload empty 2D geometry");
        return upload_geometry(std::span<const CE::Rendering::Geometry::Vertex2D>{vertices.get(), vertex_count}, topology);
    }

    Provider::~Provider() {
        if (!CE::Assets::Cache::Context::begin_provider_release(*this))
            return;

        // Drop assets which retain images and geometry before their image cache.
        // External shared owners can outlive the cache; their GPU handles still
        // belong to the renderer's shutdown sweep.
        if (auto* manager = CE::Assets::Materials::Manager::get_existing())
            manager->clear_assets();
        if (auto* manager = CE::Assets::Sprites::Manager::get_existing())
            manager->clear_assets();
        if (auto* manager = CE::Assets::Tilesets::Manager::get_existing())
            manager->clear_assets();
        if (auto* manager = CE::Text::Legacy::Manager::get_existing())
            manager->clear_assets();
        if (auto* manager = CE::Assets::Shaders::Manager::get_existing())
            manager->clear_assets();
        if (auto* manager = CE::Assets::Shaders::ProgramCache::get_existing())
            manager->clear_assets();
        if (auto* manager = CE::Assets::Images::Manager::get_existing())
            manager->clear_assets();
        CE::Assets::Cache::Context::release_provider(*this);
    }
}

namespace CE::Assets::Cache {
    std::mutex Context::provider_mutex_;
    const CE::Rendering::Resources::Provider* Context::bound_provider_ = nullptr;
    std::thread::id Context::owner_;
    bool Context::releasing_ = false;
}
