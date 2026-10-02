#include <assets/resources/resource-provider.h>

#include <core/resources/asset-management/font-mgr.h>
#include <core/resources/asset-management/material-mgr.h>
#include <core/resources/asset-management/shader-mgr.h>
#include <core/resources/asset-management/sprite-mgr.h>
#include <core/resources/asset-management/texture-mgr.h>
#include <core/resources/asset-management/tileset-mgr.h>
#include <internals/exceptions.h>

namespace CE::Assets {
    std::shared_ptr<Image> ResourceProvider::load_image(
        const std::filesystem::path& file
    ) {
        return create_image(decode_image(file));
    }

    std::shared_ptr<Geometry2D> ResourceProvider::upload_geometry(
        std::shared_ptr<Vertex2D> vertices,
        const std::uint32_t vertex_count,
        const PrimitiveTopology topology
    ) {
        if (!vertices || vertex_count == 0)
            throw Exceptions::invalid_args(CE_HERE, "Cannot upload empty 2D geometry");
        return upload_geometry(std::span<const Vertex2D>{vertices.get(), vertex_count}, topology);
    }

    ResourceProvider::~ResourceProvider() {
        if (!AssetCacheContext::begin_provider_release(*this))
            return;

        // Drop assets which retain images and geometry before their image cache.
        // External shared owners can outlive the cache; their GPU handles still
        // belong to the renderer's shutdown sweep.
        if (auto* manager = MaterialMgr::get_existing())
            manager->clear_assets();
        if (auto* manager = SpriteMgr::get_existing())
            manager->clear_assets();
        if (auto* manager = TilesetMgr::get_existing())
            manager->clear_assets();
        if (auto* manager = FontMgr::get_existing())
            manager->clear_assets();
        if (auto* manager = ShaderMgr::get_existing())
            manager->clear_assets();
        if (auto* manager = TextureMgr::get_existing())
            manager->clear_assets();
        AssetCacheContext::release_provider(*this);
    }
}
