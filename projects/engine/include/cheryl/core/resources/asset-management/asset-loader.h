#pragma once

#include <cheryl/export.h>
#include <assets/definitions/manifest.h>
#include <assets/definitions/shader-assets.h>
#include <assets/resources/decoded-image.h>
#include <templates/singleton.h>
#include <core/diagnostics.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace CE::Assets::Loading {
    class FileRegistry;
}

namespace CE::Rendering::Resources {
    struct Provider;
}

namespace CE::Assets::Loading {
    /** CPU staging image and exact intended cache key; no provider/native handle is retained. */
    struct ImageInput {
        std::filesystem::path key; ///< Intended image-cache path; automatic discovery uses normalized absolute paths.
        CE::Assets::Images::Decoded pixels; ///< Independent decoded RGBA8 pixels consumed by upload, not reread from key.
    };
    /** Owned transferable CPU batch; preparation changes no runtime caches or registry.
     * @see [Staged loading](cheryl-guide:assets/asset-loading)
     */
    struct Prepared {
        std::vector<CE::Assets::Manifests::Graphics> manifests; ///< Validated selected graphics definitions, published as metadata only after successful upload.
        std::vector<ImageInput> images; ///< Owned pixel staging with exact upload/cache keys.
        CE::Diagnostics::DomainId batch = CE::Diagnostics::next_domain_id(); ///< Diagnostic identity carried from preparation through upload observations.
        // Immutable, batch-owned discovery; preparation never changes the runtime registry.
        std::shared_ptr<const FileRegistry> files; ///< Immutable batch discovery merged into the runtime registry during upload.
        std::vector<CE::Assets::Shaders::Manifest> shaders; ///< Selected recipes with owned source-byte snapshots.
    };

    /** Copied counters for one last upload attempt; completion counters are not an atomic cache snapshot. */
    struct UploadStats {
        CE::Diagnostics::DomainId batch = 0; ///< Prepared batch diagnostic identity.
        CE::Diagnostics::DomainId provider = 0; ///< Provider diagnostic identity used by this attempt.
        std::uint64_t images_completed = 0; ///< Image entries processed successfully, including reused existing keys.
        std::uint64_t manifests_completed = 0; ///< Graphics documents whose sprite/tileset loading completed.
        std::uint64_t publications = 0; ///< New typed cache entries observed during this attempt, when available.
        bool publication_count_available = true; ///< False when publication-counter snapshots could not establish valid deltas.
        bool completed = false; ///< True only after upload and retained metadata publication finish.
        std::uint64_t shader_manifests_completed = 0; ///< Shader documents completed as a batch before image/grid loading.
        std::uint64_t replacements = 0; ///< Shader catalogue replacements observed during this attempt.
    };

    /** One immutable root; owned loaders can share the one supported cache domain.
     * CPU preparation changes no caches and can run on workers. Upload/publication
     * must be serialized on the active provider's loading owner. Relative roots
     * are interpreted against the working directory at preparation time.
     * Automatic discovery uses normalized absolute file/image keys.
     * This loader does not create a separate provider domain or atomic hot reload.
     */
    struct Loader final : CE::State::Singleton<Loader> {
    private:
        const std::filesystem::path root_path_;
        std::atomic<std::shared_ptr<const std::vector<CE::Assets::Manifests::Graphics>>> manifests_{
            std::make_shared<const std::vector<CE::Assets::Manifests::Graphics>>()
        };
        std::atomic<std::shared_ptr<const std::vector<CE::Assets::Shaders::Manifest>>> shaders_{
            std::make_shared<const std::vector<CE::Assets::Shaders::Manifest>>()
        };
        mutable std::mutex observations_mutex_;
        UploadStats last_upload_;

        [[nodiscard]] CE_API Prepared prepare_selected(const std::vector<std::filesystem::path>& indexes, bool all_images) const;

    public:
        explicit Loader(const std::filesystem::path& root_path)
        : root_path_(root_path.lexically_normal()) {}
        [[nodiscard]] CE_API Prepared prepare() const;
        [[nodiscard]] CE_API Prepared prepare_graphics(const std::vector<std::filesystem::path>& indexes) const;
        CE_API void upload(Prepared prepared, CE::Rendering::Resources::Provider& provider, bool replace_shaders = false);
        CE_API void load_assets(CE::Rendering::Resources::Provider& provider);
        CE_API void register_files() const;
        [[nodiscard]] CE_API UploadStats diagnostics() const;
        [[nodiscard]] std::shared_ptr<const std::vector<CE::Assets::Manifests::Graphics>> manifests() const {
            return manifests_.load(std::memory_order_acquire);
        }
        [[nodiscard]] std::shared_ptr<const std::vector<CE::Assets::Shaders::Manifest>> shader_manifests() const {
            return shaders_.load(std::memory_order_acquire);
        }
        CE_API static Loader& get(const std::filesystem::path& root_path);
        CE_API static Loader& get();
    };
}

namespace CE::State {
    template <> CE_API auto Singleton<CE::Assets::Loading::Loader>::storage() -> Storage&;
}

/**
 * @fn CE::Assets::Loading::Loader::Loader(const std::filesystem::path& root_path)
 * Configure an owned loader without discovering files or uploading resources.
 * @param root_path Copied, lexically normalized asset root; relative roots
 * resolve against the working directory when preparation or discovery runs.
 */

/**
 * @fn CE::Assets::Loading::Prepared CE::Assets::Loading::Loader::prepare() const
 * Rescan the root, decode its PNG images and validate graphics definitions.
 * CPU-only preparation can run on a worker. Discovery, decoding or validation
 * failure leaves caches and this loader's retained metadata unchanged.
 * @return Owned definitions, shader sources, pixels and file discovery for upload().
 */

/**
 * @fn CE::Assets::Loading::Prepared CE::Assets::Loading::Loader::prepare_graphics(const std::vector<std::filesystem::path>& indexes) const
 * Prepare selected graphics indexes against full-root discovery.
 * Decode only referenced images; a shader-only selection decodes none.
 * Empty selections and discovery, decoding or validation failures reject
 * preparation without publishing cache entries or retained metadata.
 * @param indexes Nonempty index selection; relative paths resolve against the asset root.
 * @return Owned data for upload(), prepared without a provider or backend calls.
 */

/**
 * @fn void CE::Assets::Loading::Loader::upload(Prepared prepared, CE::Rendering::Resources::Provider& provider, bool replace_shaders)
 * Consumes one prepared value without reopening its image files. Existing
 * keys remain; each successful new entry can become visible immediately.
 * Failure can leave any completed cache entries, including all entries if
 * final metadata allocation fails. manifests() retains the previous snapshot.
 * Retry by preparing again or by keeping a copy before moving the input;
 * a consumed failed value is not returned. Retry preserves existing keys,
 * not replacement of their contents. Pending platform requests may cancel
 * before execution; once executing, upload has no rollback/cancellation API.
 * Explicit shader replacement leaves image/grid keys intact. Earlier
 * successful replacements remain visible if a later candidate fails.
 * @param prepared Owned batch consumed by value; retain a copy when retry needs it.
 * @param provider Active resource backend, used on its loading owner/current context.
 * @param replace_shaders Whether existing shader programs/materials may be replaced.
 */

/**
 * @fn void CE::Assets::Loading::Loader::load_assets(CE::Rendering::Resources::Provider& provider)
 * Prepare the complete root and synchronously upload it without shader replacement.
 * Preparation failures leave caches unchanged; upload failures retain the
 * partial-publication behavior of upload().
 * @param provider Active backend, used on its loading owner/current context.
 */

/**
 * @fn void CE::Assets::Loading::Loader::register_files() const
 * Discover and register unmanaged paths for manual asset loading.
 * No JSON parsing, image decoding or provider is required. Filesystem and
 * allocation failures propagate; PNG images remain outside the untyped registry.
 */

/**
 * @fn CE::Assets::Loading::UploadStats CE::Assets::Loading::Loader::diagnostics() const
 * Observe the last upload attempt, including partial publication.
 * The snapshot is synchronized, but provides no upload/rollback ordering
 * guarantee beyond the loading-owner contract.
 * @return Copied counters and completion status for the last attempt.
 */

/**
 * @fn auto CE::Assets::Loading::Loader::manifests() const -> std::shared_ptr<const std::vector<CE::Assets::Manifests::Graphics>>
 * @return Owned snapshot of the last successfully submitted asset definitions.
 * Readers retain it across later uploads and loader destruction. This is
 * metadata, not an atomic snapshot of the global caches.
 */

/**
 * @fn auto CE::Assets::Loading::Loader::shader_manifests() const -> std::shared_ptr<const std::vector<CE::Assets::Shaders::Manifest>>
 * @return Owned snapshot of the last successfully submitted shader definitions,
 * with the same lifetime and cache-observation limits as manifests().
 */

/**
 * @fn CE::Assets::Loading::Loader& CE::Assets::Loading::Loader::get(const std::filesystem::path& root_path)
 * Configure or retrieve the legacy singleton on the loading owner before producers start.
 * A different normalized root is rejected. Prefer owned Loader instances
 * for separate roots/lifetimes; they still share the supported cache domain.
 * @param root_path Root to initialize, or match against the existing singleton.
 * @return Borrowed process-lifetime loader with that root.
 */

/**
 * @fn CE::Assets::Loading::Loader& CE::Assets::Loading::Loader::get()
 * Retrieve the configured legacy singleton without initializing it.
 * @return Borrowed process-lifetime loader; fails if initialization is unpublished.
 */
