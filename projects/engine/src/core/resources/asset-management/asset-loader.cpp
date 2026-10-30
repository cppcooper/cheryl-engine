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

namespace CE::Assets::Loading {
    namespace {
        namespace fs = std::filesystem;

        CE::Files::FileIndex discover_files(const std::filesystem::path& root) {
            const auto absolute_root = std::filesystem::absolute(root).lexically_normal();
            if (!std::filesystem::is_directory(absolute_root))
                throw Exceptions::runtime_exception(CE_HERE, "Asset root is not a directory: " + root.string());
            return CE::Files::FileIndex(absolute_root);
        }

        std::shared_ptr<const FileRegistry> prepare_file_registry(const CE::Files::FileIndex& files) {
            auto registry = std::make_shared<FileRegistry>();
            for (const auto& [extension, paths] : files.files_by_type()) {
                // PNGs enter the typed image cache; other files remain paths for
                // JSON loading, audio playback or application-specific consumers.
                if (extension != ".png")
                    registry->register_files(paths);
            }
            return registry;
        }

        void register_id(
            std::unordered_map<std::string, std::filesystem::path>& ids,
            const std::string& id,
            const std::filesystem::path& source
        ) {
            if (const auto previous = ids.find(id); previous != ids.end())
                throw Exceptions::runtime_exception(
                    CE_HERE,
                    "Duplicate asset ID '" + id + "' in manifests '" + previous->second.string() + "' and '" + source.string() + "'"
                );
            ids.emplace(id, source);
        }

        void validate_grid_bounds(
            const CE::Assets::Grids::Definition& grid,
            const std::filesystem::path& texture,
            const CE::Rendering::Geometry::PixelSize dimensions,
            const std::string& id
        ) {
            if (grid.occupied_right() > dimensions.width || grid.occupied_bottom() > dimensions.height)
                throw Exceptions::runtime_exception(CE_HERE, "Asset '" + id + "' grid exceeds texture '" + texture.string() + "' bounds");
        }

        void validate_shader_definitions(const Prepared& prepared) {
            std::unordered_map<std::string, std::filesystem::path> ids;
            std::unordered_map<std::string, const CE::Assets::Shaders::ProgramRecipe*> programs;
            std::unordered_map<std::string, const CE::Assets::Shaders::MaterialRecipe*> materials;
            for (const auto& manifest : prepared.shaders) {
                for (const auto& program : manifest.programs) {
                    register_id(ids, program.id, manifest.source);
                    CE::Assets::Shaders::validate_program(program);
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
                    CE::Assets::Shaders::validate_material(material, *program->second);
                }
            const auto selection = [&](const auto& definition, const std::filesystem::path& source) {
                if (!definition.shader)
                    return;
                const auto material = materials.find(*definition.shader);
                if (material == materials.end())
                    throw Exceptions::runtime_exception(CE_HERE, "Asset '" + definition.id() + "' in '" + source.string() +
                        "' references an unselected shader/material '" + *definition.shader + "'");
                if (material->second->vertex_layout != CE::Rendering::Geometry::Layout2D::Position3UV2 ||
                    material->second->topology != CE::Rendering::Geometry::Topology::TriangleStrip)
                    throw Exceptions::invalid_args(CE_HERE, "Asset '" + definition.id() + "' selects a shader incompatible with its grid geometry");
            };
            for (const auto& manifest : prepared.manifests) {
                for (const auto& sprite : manifest.sprites)
                    selection(sprite, manifest.source);
                for (const auto& tileset : manifest.tilesets)
                    selection(tileset, manifest.source);
            }
        }

        void snapshot_shader_sources(Prepared& prepared) {
            std::unordered_map<std::filesystem::path, std::string> bytes;
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
        auto& loader = CE::State::Singleton<Loader>::get(root_path);
        if (loader.root_path_ != root_path.lexically_normal())
            throw Exceptions::failed_operation(CE_HERE, "The singleton loader already has another root; construct an owned Loader");
        return loader;
    }

    Loader& Loader::get() {
        auto* loader = CE::State::Singleton<Loader>::get_existing();
        if (!loader)
            throw Exceptions::failed_operation(CE_HERE, "Initialize the singleton loader with an asset root first");
        return *loader;
    }

    Prepared Loader::prepare() const {
        return prepare_selected({}, true);
    }

    Prepared Loader::prepare_graphics(const std::vector<std::filesystem::path>& indexes) const {
        if (indexes.empty())
            throw Exceptions::invalid_args(CE_HERE, "Focused graphics preparation requires an explicit index selection");
        std::vector<std::filesystem::path> selected;
        const auto root = std::filesystem::absolute(root_path_).lexically_normal();
        for (const auto& index : indexes)
            selected.push_back(index.is_absolute() ? index : root / index);
        return prepare_selected(selected, false);
    }

    Prepared Loader::prepare_selected(const std::vector<std::filesystem::path>& indexes, const bool all_images) const {
        Prepared result;
        const auto started = std::chrono::steady_clock::now();
        CE_LOG_INFO(CE::Logging::Categories::assets, "subsystem=assets domain={} operation=prepare_begin", result.batch);
        const auto failed = [&] {
            Diagnostics::report_outcome("assets", result.batch, "prepare", "failed");
            CE_LOG_ERROR(CE::Logging::Categories::assets, "subsystem=assets domain={} operation=prepare outcome=failed", result.batch);
        };
        try {
            auto files = discover_files(root_path_);
            result.files = prepare_file_registry(files);
            auto definitions = CE::Assets::Manifests::Reader::load_graphics_definitions(*result.files, indexes);
            result.manifests = std::move(definitions.assets);
            result.shaders = std::move(definitions.shaders);
            validate_shader_definitions(result);
            snapshot_shader_sources(result);

            std::unordered_map<std::string, std::filesystem::path> ids;
            std::unordered_set<std::filesystem::path> images;
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
            std::vector<std::filesystem::path> ordered(images.begin(), images.end());
            std::ranges::sort(ordered);
            std::unordered_map<std::filesystem::path, CE::Rendering::Geometry::PixelSize> dimensions;
            for (const auto& image : ordered) {
                auto pixels = CE::Assets::Images::decode(image);
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
                CE::Logging::Categories::assets, "subsystem=assets domain={} operation=prepare_end manifests={} images={} duration_us={}",
                result.batch, result.manifests.size(), result.images.size(),
                std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count()
            );
            return result;
        } catch (const std::filesystem::filesystem_error& error) {
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

    std::uint64_t Loader::next_event_identity() {
        const auto identity = Diagnostics::next_domain_id();
        if (!identity)
            throw Exceptions::failed_operation(CE_HERE, "Upload observation identities exhausted");
        return identity;
    }

    void Loader::publish_events_to(CE::Events::EventBus::Publisher publisher) {
        if (uploading_ || publishing_events_ || !pending_events_.empty())
            throw Exceptions::failed_operation(CE_HERE, "Upload notification routing requires an idle, drained loader");
        publisher_ = std::move(publisher);
    }

    void Loader::settle_upload(
        const std::uint64_t attempt,
        const UploadStats& counts,
        const bool effects_possible,
        std::exception_ptr failure
    ) noexcept {
        if (publisher_.expired())
            return;
        try {
            auto outcome = UploadOutcome::Succeeded;
            if (failure) {
                if (counts.publication_count_available && (counts.publications || counts.replacements))
                    outcome = UploadOutcome::Partial;
                else if (!effects_possible)
                    outcome = UploadOutcome::FailedBeforeEffects;
                else
                    outcome = UploadOutcome::EffectsUnknown;
            }
            // One record per attempt. A slow/reentrant observer cannot grow an
            // unbounded history or change the direct upload result on overflow.
            if (pending_events_.size() == 64)
                throw Exceptions::failed_operation(CE_HERE, "Pending upload observations exhausted");
            pending_events_.push_back({source_, attempt, std::chrono::steady_clock::now(), outcome, counts, std::move(failure)});
        } catch (...) {
            Diagnostics::report_failure("upload outcome staging", std::current_exception());
        }
        publish_pending_events();
    }

    void Loader::publish_pending_events() noexcept {
        if (publishing_events_ || uploading_)
            return;
        publishing_events_ = true;
        const auto cutoff = pending_events_.size();
        for (std::size_t index = 0; index < cutoff; ++index) {
            auto event = std::move(pending_events_.front());
            pending_events_.erase(pending_events_.begin());
            const auto outcome = event.outcome == UploadOutcome::Succeeded             ? "succeeded"
                                 : event.outcome == UploadOutcome::FailedBeforeEffects ? "failed_before_effects"
                                 : event.outcome == UploadOutcome::Partial             ? "partial"
                                                                                       : "effects_unknown";
            CE_LOG_DEBUG(
                CE::Logging::Categories::assets,
                "subsystem=events event=asset-upload-settled operation=notification_attempt source={} attempt={} batch={} provider={} outcome={} images={} manifests={} shader_manifests={} publications={} replacements={} counts_known={} publisher_expired={}",
                event.source, event.attempt, event.counts.batch, event.counts.provider, outcome, event.counts.images_completed,
                event.counts.manifests_completed, event.counts.shader_manifests_completed, event.counts.publications,
                event.counts.replacements, event.counts.publication_count_available, publisher_.expired()
            );
            publisher_.notify(asset_upload_settled_event, event);
        }
        publishing_events_ = false;
    }

    void Loader::upload(Prepared prepared, CE::Rendering::Resources::Provider& provider, const bool replace_shaders) {
        if (uploading_)
            throw Exceptions::failed_operation(CE_HERE, "Upload attempts require serialized execution on the loading owner");
        const auto attempt = next_event_identity();
        UploadStats observed{prepared.batch, provider.diagnostic_id()};
        bool effects_possible = false;
        const auto started = std::chrono::steady_clock::now();
        const auto publications = []() noexcept -> std::optional<std::pair<std::uint64_t, std::uint64_t>> {
            try {
                std::uint64_t result = 0;
                if (const auto* cache = CE::Assets::Images::Manager::get_existing())
                    result += cache->diagnostics().publications;
                if (const auto* cache = CE::Assets::Sprites::Manager::get_existing())
                    result += cache->diagnostics().publications;
                if (const auto* cache = CE::Assets::Tilesets::Manager::get_existing())
                    result += cache->diagnostics().publications;
                std::uint64_t replacements = 0;
                if (const auto* cache = CE::Assets::Shaders::Manager::get_existing()) {
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
                    CE::Logging::Categories::assets,
                    "subsystem=assets domain={} provider={} operation=upload_end images={} manifests={} shader_manifests={} publications={} replacements={} duration_us={}",
                    observed.batch, observed.provider, observed.images_completed, observed.manifests_completed,
                    observed.shader_manifests_completed, observed.publications, observed.replacements,
                    std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count()
                );
            } else {
                const auto outcome = !observed.publication_count_available ? "publication_unknown" :
                                     observed.publications || observed.replacements ? "partial" : "failed";
                Diagnostics::report_outcome("assets", observed.batch, "upload", outcome, observed.publications + observed.replacements);
                CE_LOG_ERROR(
                    CE::Logging::Categories::assets,
                    "subsystem=assets domain={} provider={} operation=upload outcome={} images={} manifests={} shader_manifests={} publications={} replacements={} count_available={}",
                    observed.batch, observed.provider, outcome, observed.images_completed, observed.manifests_completed,
                    observed.shader_manifests_completed, observed.publications, observed.replacements, observed.publication_count_available
                );
            }
        };
        CE_LOG_INFO(
            CE::Logging::Categories::assets, "subsystem=assets domain={} provider={} operation=upload_begin images={} manifests={}",
            observed.batch, observed.provider, prepared.images.size(), prepared.manifests.size()
        );
        uploading_ = true;
        try {
            CE::Assets::Cache::Context::verify_provider(provider);
            before = publications();
            verified = true;
            validate_shader_definitions(prepared);
            CE::Assets::Shaders::Manager::get().validate(prepared.shaders, provider, replace_shaders);
            if (prepared.files) {
                effects_possible = true;
                FileRegistry::get().register_files(*prepared.files);
            }
            if (!prepared.shaders.empty())
                effects_possible = true;
            CE::Assets::Shaders::Manager::get().load(prepared.shaders, provider, replace_shaders);
            observed.shader_manifests_completed = prepared.shaders.size();
            for (const auto& image : prepared.images) {
                effects_possible = true;
                CE::Assets::Images::Manager::get().upload(image.key, image.pixels, provider);
                ++observed.images_completed;
            }
            for (const auto& manifest : prepared.manifests) {
                effects_possible = true;
                CE::Assets::Sprites::Manager::get().load(manifest.sprites, provider);
                CE::Assets::Tilesets::Manager::get().load(manifest.tilesets, provider);
                ++observed.manifests_completed;
            }
            // Upload failure can leave completed cache entries, but never publishes partial metadata.
            const auto manifests = std::make_shared<const std::vector<CE::Assets::Manifests::Graphics>>(std::move(prepared.manifests));
            const auto shaders = std::make_shared<const std::vector<CE::Assets::Shaders::Manifest>>(std::move(prepared.shaders));
            manifests_.store(manifests, std::memory_order_release);
            shaders_.store(shaders, std::memory_order_release);
        } catch (...) {
            const auto failure = std::current_exception();
            finish(false);
            uploading_ = false;
            settle_upload(attempt, observed, effects_possible, failure);
            throw;
        }
        finish(true);
        uploading_ = false;
        settle_upload(attempt, observed, effects_possible, {});
    }

    void Loader::load_assets(CE::Rendering::Resources::Provider& provider) {
        CE::Assets::Cache::Context::verify_provider(provider);
        upload(prepare(), provider);
    }

    void Loader::register_files() const {
        try {
            const auto files = discover_files(root_path_);
            FileRegistry::get().register_files(*prepare_file_registry(files));
        } catch (const std::filesystem::filesystem_error& error) {
            throw Exceptions::runtime_exception(CE_HERE, error.what());
        }
    }
}

namespace CE::State {
    template <> auto Singleton<CE::Assets::Loading::Loader>::storage() -> Storage& {
        static Storage value;
        return value;
    }
}
