#include <assets/resources/resource-provider.h>
#include <core/resources/asset-management/asset-loader.h>
#include <core/resources/asset-management/manifest-loader.h>
#include <core/resources/asset-management/sprite-mgr.h>
#include <core/resources/asset-management/texture-mgr.h>
#include <core/resources/asset-management/tileset-mgr.h>
#include <internals/exceptions.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace CE::Assets {
    namespace {
        namespace fs = std::filesystem;

        std::string lowercase(std::string value) {
            std::ranges::transform(value, value.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        }

        void register_id(std::unordered_map<std::string, fs::path>& ids, const std::string& id, const fs::path& source) {
            if (const auto previous = ids.find(id); previous != ids.end())
                throw Exceptions::runtime_exception(CE_HERE,
                                                    "Duplicate asset ID '" + id + "' in manifests '" + previous->second.string() +
                                                    "' and '" + source.string() + "'");
            ids.emplace(id, source);
        }

        void validate_grid_bounds(const GridDefinition& grid, const fs::path& texture, const PixelSize dimensions, const std::string& id) {
            if (grid.occupied_right() > dimensions.width || grid.occupied_bottom() > dimensions.height)
                throw Exceptions::runtime_exception(CE_HERE, "Asset '" + id + "' grid exceeds texture '" + texture.string() + "' bounds");
        }
    }

    Loader& Loader::get(const std::filesystem::path& root_path) {
        auto& loader = Singleton_CTS<Loader>::get(root_path);
        if (loader.root_path_ != root_path.lexically_normal())
            throw Exceptions::failed_operation(CE_HERE, "The singleton loader already has another root; construct an owned Loader");
        return loader;
    }

    Loader& Loader::get() {
        auto* loader = Singleton_CTS<Loader>::get_existing();
        if (!loader)
            throw Exceptions::failed_operation(CE_HERE, "Initialize the singleton loader with an asset root first");
        return *loader;
    }

    PreparedAssets Loader::prepare() const {
        try {
            if (!fs::is_directory(root_path_))
                throw Exceptions::runtime_exception(CE_HERE, "Asset root is not a directory: " + root_path_.string());
            PreparedAssets result;
            std::vector<fs::path> documents;
            // A fresh scan sees added files; schema files below the root are not manifests.
            for (const auto& entry : fs::directory_iterator(root_path_))
                if (entry.is_regular_file() && lowercase(entry.path().extension().string()) == ".json")
                    documents.push_back(entry.path().lexically_normal());
            std::ranges::sort(documents);
            for (const auto& document : documents)
                result.manifests.push_back(ManifestLoader::load(document));

            std::unordered_map<std::string, fs::path> ids;
            std::unordered_set<fs::path> images;
            for (const auto& manifest : result.manifests) {
                for (const auto& sprite : manifest.sprites) {
                    register_id(ids, sprite.id(), manifest.source);
                    images.insert(sprite.texture);
                }
                for (const auto& tileset : manifest.tilesets) {
                    register_id(ids, tileset.id(), manifest.source);
                    images.insert(tileset.texture);
                }
            }
            for (const auto& entry : fs::recursive_directory_iterator(root_path_))
                if (entry.is_regular_file() && lowercase(entry.path().extension().string()) == ".png")
                    images.insert(entry.path().lexically_normal());
            std::vector<fs::path> ordered(images.begin(), images.end());
            std::ranges::sort(ordered);
            std::unordered_map<fs::path, PixelSize> dimensions;
            for (const auto& image : ordered) {
                auto pixels = decode_image(image);
                dimensions.emplace(image, pixels.size);
                result.images.push_back({image, std::move(pixels)});
            }
            // Validate against the exact owned pixels that upload will receive.
            for (const auto& manifest : result.manifests) {
                for (const auto& sprite : manifest.sprites)
                    validate_grid_bounds(sprite.grid, sprite.texture, dimensions.at(sprite.texture), sprite.id());
                for (const auto& tileset : manifest.tilesets)
                    validate_grid_bounds(tileset.grid, tileset.texture, dimensions.at(tileset.texture), tileset.id());
            }
            return result;
        }
        catch (const fs::filesystem_error& error) {
            throw Exceptions::runtime_exception(CE_HERE, error.what());
        }
    }

    void Loader::upload(PreparedAssets prepared, ResourceProvider& provider) {
        ProviderBoundCache::verify_provider(provider);
        for (const auto& image : prepared.images)
            TextureMgr::get().load_asset(image.key, image.pixels, provider);
        for (const auto& manifest : prepared.manifests) {
            SpriteMgr::get().load_assets(manifest.sprites, provider);
            TilesetMgr::get().load_assets(manifest.tilesets, provider);
        }
        // Upload failure can leave completed cache entries, but never publishes partial metadata.
        manifests_.store(std::make_shared<const std::vector<AssetManifest>>(std::move(prepared.manifests)), std::memory_order_release);
    }

    void Loader::load_assets(ResourceProvider& provider) {
        ProviderBoundCache::verify_provider(provider);
        upload(prepare(), provider);
    }
}
