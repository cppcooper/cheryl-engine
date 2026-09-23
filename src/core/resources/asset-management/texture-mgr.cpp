#include <core/resources/asset-management/texture-mgr.h>

#include <assets/abstracts/resource-provider.h>
#include <internals/exceptions.h>

namespace CE::Assets {
    TextureMgr::spointer TextureMgr::get_asset(const std::filesystem::path& file) const {
        const auto normalized = file.lexically_normal();
        if (const auto exact = loaded_assets.find(normalized); exact != loaded_assets.end()) {
            return exact->second;
        }
        if (file.has_parent_path()) {
            return nullptr;
        }

        spointer result;
        for (const auto& [path, texture] : loaded_assets) {
            if (path.filename() != file) {
                continue;
            }
            if (result) {
                throw Exceptions::runtime_exception(
                    CE_HERE, "Texture filename '" + file.string() + "' is ambiguous; use its resolved path");
            }
            result = texture;
        }
        return result;
    }

    void TextureMgr::load_assets(const std::vector<std::filesystem::path>& files, ResourceProvider& provider) {
        bind_provider(provider);
        for (const auto& requested : files) {
            const auto file = requested.lexically_normal();
            if (!loaded_assets.contains(file)) {
                loaded_assets.emplace(file, provider.load_image(file));
            }
        }
    }
}
