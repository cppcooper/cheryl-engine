#include <core/resources/asset-management/texture-mgr.h>

#include <assets/resources/resource-provider.h>
#include <internals/exceptions.h>

namespace CE::Assets::Images {
    void Manager::upload(const std::filesystem::path& file, const Decoded& image, CE::Rendering::Resources::Provider& provider) {
        bind_provider(provider);
        const auto key = file.lexically_normal();
        if (!contains(key))
            publish_asset(key, provider.create_image(image));
    }

    Manager::Handle Manager::get_asset(const std::filesystem::path& file) const {
        // Prefer the resolved path used at load time; a bare filename is a convenience
        // lookup only when it uniquely identifies one cached texture.
        const auto normalized = file.lexically_normal();
        std::shared_lock lock(assets_mutex_);
        if (const auto exact = loaded_assets.find(normalized); exact != loaded_assets.end()) {
            return exact->second;
        }
        if (file.has_parent_path()) {
            return nullptr;
        }

        Handle result;
        for (const auto& [path, texture] : loaded_assets) {
            if (path.filename() != file) {
                continue;
            }
            if (result) {
                throw Exceptions::runtime_exception(
                    CE_HERE, "Texture filename '" + file.string() + "' is ambiguous; use its resolved path"
                );
            }
            result = texture;
        }
        return result;
    }

    void Manager::load(const std::vector<std::filesystem::path>& files, CE::Rendering::Resources::Provider& provider) {
        bind_provider(provider);
        for (const auto& requested : files) {
            const auto file = requested.lexically_normal();
            if (!contains(file)) {
                publish_asset(file, provider.load_image(file));
            }
        }
    }
}

namespace CE::State {
    template <> auto Singleton<CE::Assets::Images::Manager>::storage() -> Storage& {
        static Storage value;
        return value;
    }
}
