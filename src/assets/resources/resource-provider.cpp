#include <assets/resources/resource-provider.h>

#include <core/resources/asset-management/font-mgr.h>
#include <core/resources/asset-management/shader-mgr.h>
#include <core/resources/asset-management/sprite-mgr.h>
#include <core/resources/asset-management/texture-mgr.h>
#include <core/resources/asset-management/tileset-mgr.h>

namespace CE::Assets {
    ResourceProvider::~ResourceProvider() {
        if (!ProviderBoundCache::is_bound_to(*this)) return;

        // Drop assets which retain images and geometry before their image cache.
        // External shared owners can outlive the cache; their GPU handles still
        // belong to the renderer's shutdown sweep.
        if (auto* manager = SpriteMgr::get_existing()) manager->clear_assets();
        if (auto* manager = TilesetMgr::get_existing()) manager->clear_assets();
        if (auto* manager = FontMgr::get_existing()) manager->clear_assets();
        if (auto* manager = ShaderMgr::get_existing()) manager->clear_assets();
        if (auto* manager = TextureMgr::get_existing()) manager->clear_assets();
        ProviderBoundCache::release_provider(*this);
    }
}
