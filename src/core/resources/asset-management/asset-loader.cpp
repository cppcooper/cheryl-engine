#include <core/resources/asset-management/asset-loader.h>

#include <core/resources/asset-management.h>
#include <assets/abstracts/resource-provider.h>
#include <core/resources/fileio/fonts-system.h>
#include <internals/exceptions.h>
#include <stb_image.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace CE::Assets {
    namespace {
        namespace fs = std::filesystem;

        std::string lowercase(std::string value) {
            std::ranges::transform(value, value.begin(), [](const unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });
            return value;
        }

        void register_id(std::unordered_map<std::string, fs::path>& ids, const std::string& id,
                         const fs::path& source) {
            if (const auto existing = ids.find(id); existing != ids.end()) {
                throw Exceptions::runtime_exception(CE_HERE,
                                                    "Duplicate asset ID '" + id + "' in manifests '" +
                                                        existing->second.string() + "' and '" + source.string() + "'");
            }
            ids.emplace(id, source);
        }

        std::pair<int, int> inspect_texture(const fs::path& texture) {
            if (!fs::is_regular_file(texture)) {
                throw Exceptions::runtime_exception(
                    CE_HERE, "Manifest texture does not exist or is not a file: '" + texture.string() + "'");
            }
            int width{};
            int height{};
            int channels{};
            if (stbi_info(texture.string().c_str(), &width, &height, &channels) == 0 || width <= 0 || height <= 0) {
                throw Exceptions::runtime_exception(
                    CE_HERE, "Unable to read manifest texture metadata from '" + texture.string() + "'");
            }
            return {width, height};
        }

        void validate_grid_bounds(const GridDefinition& grid, const fs::path& texture,
                                  const std::pair<int, int> dimensions, const std::string& asset_id) {
            if (grid.occupied_right() > static_cast<std::uint64_t>(dimensions.first) ||
                grid.occupied_bottom() > static_cast<std::uint64_t>(dimensions.second)) {
                throw Exceptions::runtime_exception(
                    CE_HERE,
                    "Asset '" + asset_id + "' grid exceeds texture '" + texture.string() + "' bounds (" +
                        std::to_string(dimensions.first) + 'x' + std::to_string(dimensions.second) + ')');
            }
        }
    }

    void Loader::load_assets(ResourceProvider& provider) {
        ProviderBoundCache::verify_provider(provider);
        if (!fs::is_directory(root_path_)) {
            throw Exceptions::runtime_exception(
                CE_HERE, "Asset root does not exist or is not a directory: '" + root_path_.string() + "'");
        }

        std::vector<fs::path> manifest_files;
        for (const auto& entry : fs::directory_iterator(root_path_)) {
            if (entry.is_regular_file() && lowercase(entry.path().extension().string()) == ".json") {
                manifest_files.push_back(entry.path().lexically_normal());
            }
        }
        std::ranges::sort(manifest_files);

        std::vector<AssetManifest> parsed_manifests;
        parsed_manifests.reserve(manifest_files.size());
        for (const auto& file : manifest_files) {
            parsed_manifests.push_back(ManifestLoader::load(file));
        }

        std::vector<SpriteDefinition> sprites;
        std::vector<TilesetDefinition> tilesets;
        std::vector<fs::path> referenced_textures;
        std::unordered_set<fs::path> seen_textures;
        std::unordered_map<std::string, fs::path> asset_ids;
        for (const auto& manifest : parsed_manifests) {
            for (const auto& sprite : manifest.sprites) {
                register_id(asset_ids, sprite.id(), manifest.source);
                sprites.push_back(sprite);
                if (seen_textures.emplace(sprite.texture).second) {
                    referenced_textures.push_back(sprite.texture);
                }
            }
            for (const auto& tileset : manifest.tilesets) {
                register_id(asset_ids, tileset.id(), manifest.source);
                tilesets.push_back(tileset);
                if (seen_textures.emplace(tileset.texture).second) {
                    referenced_textures.push_back(tileset.texture);
                }
            }
        }
        std::ranges::sort(referenced_textures);

        std::unordered_map<fs::path, std::pair<int, int>> texture_dimensions;
        for (const auto& texture : referenced_textures) {
            texture_dimensions.emplace(texture, inspect_texture(texture));
        }
        for (const auto& sprite : sprites) {
            validate_grid_bounds(sprite.grid, sprite.texture, texture_dimensions.at(sprite.texture), sprite.id());
        }
        for (const auto& tileset : tilesets) {
            validate_grid_bounds(tileset.grid, tileset.texture, texture_dimensions.at(tileset.texture), tileset.id());
        }

        std::vector<fs::path> textures = referenced_textures;
        for (const auto& file : get_files_of_type(".png")) {
            const auto normalized = file.lexically_normal();
            if (seen_textures.emplace(normalized).second) {
                textures.push_back(normalized);
            }
        }
        std::ranges::sort(textures);
        TextureMgr::get().load_assets(textures, provider);
        SpriteMgr::get().load_assets(sprites, provider);
        TilesetMgr::get().load_assets(tilesets, provider);

        const auto default_font = Resources::select_default_system_font(Resources::find_system_fonts());
        if (default_font) {
            FontMgr::get().load_assets({*default_font}, provider);
        }

        std::vector<fs::path> shaders;
        constexpr std::array shader_extensions{".vert", ".geo", ".frag", ".tesc", ".tese"};
        for (const auto extension : shader_extensions) {
            const auto& files = get_files_of_type(extension);
            shaders.insert(shaders.end(), files.begin(), files.end());
        }
        ShaderMgr::get().load_assets(shaders, provider);
        const auto shader2d = root_path_ / "shaders" / "shader2d";
        ShaderMgr::get().load_program(shader2d, {shader2d.string() + ".vert", shader2d.string() + ".frag"},
                                      provider);

        manifests_ = std::move(parsed_manifests);
    }
}
