#include <assets/resources/resource-provider.h>
#include <core/resources/asset-management/asset-loader.h>
#include <core/resources/asset-management/file-registry.h>
#include <core/resources/asset-management/manifest-loader.h>
#include <core/resources/asset-management/shader-asset-mgr.h>
#include <core/resources/asset-management/sprite-mgr.h>
#include <core/resources/asset-management/texture-mgr.h>
#include <core/resources/asset-management/tileset-mgr.h>
#include <core/resources/fileio/file-mgr.h>
#include <internals/exceptions.h>
#include <internals/compile-time-logging.hpp>
#include <chrono>
#include <optional>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace CE::Assets {
    namespace {
        namespace fs = std::filesystem;

        FileMgr discover_files(const fs::path& root) {
            const auto absolute_root = fs::absolute(root).lexically_normal();
            if (!fs::is_directory(absolute_root))
                throw Exceptions::runtime_exception(CE_HERE, "Asset root is not a directory: " + root.string());
            return FileMgr(absolute_root);
        }

        std::shared_ptr<const FileRegistry> prepare_file_registry(const FileMgr& files) {
            auto registry = std::make_shared<FileRegistry>();
            for (const auto& [extension, paths] : files.files_by_type()) {
                // PNGs enter the typed image cache; other files remain paths for
                // JSON loading, audio playback or application-specific consumers.
                if (extension != ".png")
                    registry->register_files(paths);
            }
            return registry;
        }

        void register_id(std::unordered_map<std::string, fs::path>& ids, const std::string& id, const fs::path& source) {
            if (const auto previous = ids.find(id); previous != ids.end())
                throw Exceptions::runtime_exception(
                    CE_HERE,
                    "Duplicate asset ID '" + id + "' in manifests '" + previous->second.string() + "' and '" + source.string() + "'"
                );
            ids.emplace(id, source);
        }

        void validate_grid_bounds(const GridDefinition& grid, const fs::path& texture, const PixelSize dimensions, const std::string& id) {
            if (grid.occupied_right() > dimensions.width || grid.occupied_bottom() > dimensions.height)
                throw Exceptions::runtime_exception(CE_HERE, "Asset '" + id + "' grid exceeds texture '" + texture.string() + "' bounds");
        }

        void validate_shader_definitions(const PreparedAssets& prepared) {
            std::unordered_map<std::string, fs::path> ids;
            std::unordered_map<std::string, const ShaderProgramRecipe*> programs;
            std::unordered_map<std::string, const ShaderMaterialRecipe*> materials;
            for (const auto& manifest : prepared.shaders) {
                for (const auto& program : manifest.programs) {
                    register_id(ids, program.id, manifest.source);
                    validate_shader_program(program);
                    programs.emplace(program.id, &program);
                }
                for (const auto& material : manifest.materials) {
                    register_id(ids, material.id, manifest.source);
                    materials.emplace(material.id, &material);
                }
            }
            for (const auto& manifest : prepared.shaders)
                for (const auto& material : manifest.materials) {
                    const auto program = programs.find(material.program);
                    if (program == programs.end())
                        throw Exceptions::runtime_exception(CE_HERE, "Shader material '" + material.id + "' in '" + manifest.source.string() +
                            "' references an unselected program '" + material.program + "'");
                    validate_shader_material(material, *program->second);
                }
            const auto selection = [&](const auto& definition, const fs::path& source) {
                if (!definition.shader)
                    return;
                const auto material = materials.find(*definition.shader);
                if (material == materials.end())
                    throw Exceptions::runtime_exception(CE_HERE, "Asset '" + definition.id() + "' in '" + source.string() +
                        "' references an unselected shader/material '" + *definition.shader + "'");
                if (material->second->vertex_layout != VertexLayout2D::Position3UV2 ||
                    material->second->topology != PrimitiveTopology::TriangleStrip)
                    throw Exceptions::invalid_args(CE_HERE, "Asset '" + definition.id() + "' selects a shader incompatible with its grid geometry");
            };
            for (const auto& manifest : prepared.manifests) {
                for (const auto& sprite : manifest.sprites)
                    selection(sprite, manifest.source);
                for (const auto& tileset : manifest.tilesets)
                    selection(tileset, manifest.source);
            }
        }

        void snapshot_shader_sources(PreparedAssets& prepared) {
            std::unordered_map<fs::path, std::string> bytes;
            for (auto& manifest : prepared.shaders)
                for (auto& program : manifest.programs)
                    for (auto& source : program.sources) {
                        const auto path = prepared.files->get_file_at(source.path);
                        if (!path)
                            throw Exceptions::runtime_exception(CE_HERE, "Shader program '" + program.id + "' in '" + manifest.source.string() +
                                "' references an unregistered source '" + source.path.string() + "'");
                        const auto found = bytes.find(*path);
                        if (found == bytes.end()) {
                            std::ifstream input(*path, std::ios::binary);
                            if (!input)
                                throw Exceptions::runtime_exception(CE_HERE, "Could not open shader source '" + path->string() + "'");
                            std::string owned(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
                            if (input.bad())
                                throw Exceptions::runtime_exception(CE_HERE, "Could not read shader source '" + path->string() + "'");
                            source.bytes = bytes.emplace(*path, std::move(owned)).first->second;
                        } else
                            source.bytes = found->second;
                        source.path = *path;
                    }
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
        return prepare_selected({}, true);
    }

    PreparedAssets Loader::prepare_graphics(const std::vector<fs::path>& indexes) const {
        if (indexes.empty())
            throw Exceptions::invalid_args(CE_HERE, "Focused graphics preparation requires an explicit index selection");
        std::vector<fs::path> selected;
        const auto root = fs::absolute(root_path_).lexically_normal();
        for (const auto& index : indexes)
            selected.push_back(index.is_absolute() ? index : root / index);
        return prepare_selected(selected, false);
    }

    PreparedAssets Loader::prepare_selected(const std::vector<fs::path>& indexes, const bool all_images) const {
        PreparedAssets result;
        const auto started = std::chrono::steady_clock::now();
        CE_LOG_INFO(CE::assetlog, "subsystem=assets domain={} operation=prepare_begin", result.batch);
        const auto failed = [&] {
            Diagnostics::report_outcome("assets", result.batch, "prepare", "failed");
            CE_LOG_ERROR(CE::assetlog, "subsystem=assets domain={} operation=prepare outcome=failed", result.batch);
        };
        try {
            auto files = discover_files(root_path_);
            result.files = prepare_file_registry(files);
            auto definitions = ManifestLoader::load_graphics_definitions(*result.files, indexes);
            result.manifests = std::move(definitions.assets);
            result.shaders = std::move(definitions.shaders);
            validate_shader_definitions(result);
            snapshot_shader_sources(result);

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
            if (all_images)
                for (const auto& image : files.get_files_of_type(".png"))
                    images.insert(image.lexically_normal());
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
            CE_LOG_INFO(
                CE::assetlog, "subsystem=assets domain={} operation=prepare_end manifests={} images={} duration_us={}", result.batch,
                result.manifests.size(), result.images.size(),
                std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count()
            );
            return result;
        } catch (const fs::filesystem_error& error) {
            failed();
            throw Exceptions::runtime_exception(CE_HERE, error.what());
        } catch (...) {
            failed();
            throw;
        }
    }

    UploadStats Loader::diagnostics() const {
        std::lock_guard lock(observations_mutex_);
        return last_upload_;
    }

    void Loader::upload(PreparedAssets prepared, ResourceProvider& provider, const bool replace_shaders) {
        UploadStats observed{prepared.batch, provider.diagnostic_id()};
        const auto started = std::chrono::steady_clock::now();
        const auto publications = []() noexcept -> std::optional<std::pair<std::uint64_t, std::uint64_t>> {
            try {
                std::uint64_t result = 0;
                if (const auto* cache = TextureMgr::get_existing())
                    result += cache->diagnostics().publications;
                if (const auto* cache = SpriteMgr::get_existing())
                    result += cache->diagnostics().publications;
                if (const auto* cache = TilesetMgr::get_existing())
                    result += cache->diagnostics().publications;
                std::uint64_t replacements = 0;
                if (const auto* cache = ShaderAssetMgr::get_existing()) {
                    const auto stats = cache->diagnostics();
                    result += stats.publications;
                    replacements = stats.replacements;
                }
                return std::pair{result, replacements};
            } catch (...) {
                Diagnostics::report_failure("asset publication counters", std::current_exception());
                return std::nullopt;
            }
        };
        std::optional<std::pair<std::uint64_t, std::uint64_t>> before;
        bool verified = false;
        const auto finish = [&](const bool completed) noexcept {
            observed.completed = completed;
            if (verified) {
                const auto after = publications();
                if (before && after && after->first >= before->first && after->second >= before->second) {
                    observed.publications = after->first - before->first;
                    observed.replacements = after->second - before->second;
                } else
                    observed.publication_count_available = false;
            }
            try {
                std::lock_guard lock(observations_mutex_);
                last_upload_ = observed;
            } catch (...) {
                Diagnostics::report_failure("asset publication observation", std::current_exception());
            }
            if (completed) {
                CE_LOG_INFO(
                    CE::assetlog,
                    "subsystem=assets domain={} provider={} operation=upload_end images={} manifests={} publications={} duration_us={}",
                    observed.batch, observed.provider, observed.images_completed, observed.manifests_completed, observed.publications,
                    std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count()
                );
            } else {
                const auto outcome = !observed.publication_count_available ? "publication_unknown" :
                                     observed.publications || observed.replacements ? "partial" : "failed";
                Diagnostics::report_outcome("assets", observed.batch, "upload", outcome, observed.publications + observed.replacements);
                CE_LOG_ERROR(
                    CE::assetlog,
                    "subsystem=assets domain={} provider={} operation=upload outcome={} images={} manifests={} publications={} count_available={}",
                    observed.batch, observed.provider, outcome, observed.images_completed, observed.manifests_completed,
                    observed.publications, observed.publication_count_available
                );
            }
        };
        CE_LOG_INFO(
            CE::assetlog, "subsystem=assets domain={} provider={} operation=upload_begin images={} manifests={}", observed.batch,
            observed.provider, prepared.images.size(), prepared.manifests.size()
        );
        try {
            AssetCacheContext::verify_provider(provider);
            before = publications();
            verified = true;
            validate_shader_definitions(prepared);
            ShaderAssetMgr::get().validate_assets(prepared.shaders, provider, replace_shaders);
            if (prepared.files)
                FileRegistry::get().register_files(*prepared.files);
            ShaderAssetMgr::get().load_assets(prepared.shaders, provider, replace_shaders);
            observed.shader_manifests_completed = prepared.shaders.size();
            for (const auto& image : prepared.images) {
                TextureMgr::get().load_asset(image.key, image.pixels, provider);
                ++observed.images_completed;
            }
            for (const auto& manifest : prepared.manifests) {
                SpriteMgr::get().load_assets(manifest.sprites, provider);
                TilesetMgr::get().load_assets(manifest.tilesets, provider);
                ++observed.manifests_completed;
            }
            // Upload failure can leave completed cache entries, but never publishes partial metadata.
            const auto manifests = std::make_shared<const std::vector<AssetManifest>>(std::move(prepared.manifests));
            const auto shaders = std::make_shared<const std::vector<ShaderAssetManifest>>(std::move(prepared.shaders));
            manifests_.store(manifests, std::memory_order_release);
            shaders_.store(shaders, std::memory_order_release);
        } catch (...) {
            finish(false);
            throw;
        }
        finish(true);
    }

    void Loader::load_assets(ResourceProvider& provider) {
        AssetCacheContext::verify_provider(provider);
        upload(prepare(), provider);
    }

    void Loader::register_files() const {
        try {
            const auto files = discover_files(root_path_);
            FileRegistry::get().register_files(*prepare_file_registry(files));
        } catch (const fs::filesystem_error& error) {
            throw Exceptions::runtime_exception(CE_HERE, error.what());
        }
    }
}
