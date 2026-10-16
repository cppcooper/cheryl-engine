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
    // Owns decoded staging pixels. No provider/native/cache handle is retained.
    struct ImageInput {
        std::filesystem::path key;
        CE::Assets::Images::Decoded pixels;
    };
    struct Prepared {
        std::vector<CE::Assets::Manifests::Graphics> manifests;
        std::vector<ImageInput> images;
        CE::Diagnostics::DomainId batch = CE::Diagnostics::next_domain_id();
        // Immutable, batch-owned discovery; preparation never changes the runtime registry.
        std::shared_ptr<const FileRegistry> files;
        std::vector<CE::Assets::Shaders::Manifest> shaders;
    };

    struct UploadStats {
        CE::Diagnostics::DomainId batch = 0;
        CE::Diagnostics::DomainId provider = 0;
        std::uint64_t images_completed = 0;
        std::uint64_t manifests_completed = 0;
        std::uint64_t publications = 0;
        bool publication_count_available = true;
        bool completed = false;
        std::uint64_t shader_manifests_completed = 0;
        std::uint64_t replacements = 0;
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
        /** Configure an owned loader without discovering files or uploading resources.
         * @param root_path Copied, lexically normalized asset root; relative roots
         * resolve against the working directory when preparation or discovery runs.
         */
        explicit Loader(const std::filesystem::path& root_path)
        : root_path_(root_path.lexically_normal()) {}
        /** Rescan the root, decode its PNG images and validate graphics definitions.
         * CPU-only preparation can run on a worker. Discovery, decoding or validation
         * failure leaves caches and this loader's retained metadata unchanged.
         * @return Owned definitions, shader sources, pixels and file discovery for upload().
         */
        [[nodiscard]] CE_API Prepared prepare() const;
        /** Prepare selected graphics indexes against full-root discovery.
         * Decode only referenced images; a shader-only selection decodes none.
         * Empty selections and discovery, decoding or validation failures reject
         * preparation without publishing cache entries or retained metadata.
         * @param indexes Nonempty index selection; relative paths resolve against the asset root.
         * @return Owned data for upload(), prepared without a provider or backend calls.
         */
        [[nodiscard]] CE_API Prepared prepare_graphics(const std::vector<std::filesystem::path>& indexes) const;
        /** Consumes one prepared value without reopening its image files. Existing
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
        CE_API void upload(Prepared prepared, CE::Rendering::Resources::Provider& provider, bool replace_shaders = false);
        /** Prepare the complete root and synchronously upload it without shader replacement.
         * Preparation failures leave caches unchanged; upload failures retain the
         * partial-publication behavior of upload().
         * @param provider Active backend, used on its loading owner/current context.
         */
        CE_API void load_assets(CE::Rendering::Resources::Provider& provider);
        /** Discover and register unmanaged paths for manual asset loading.
         * No JSON parsing, image decoding or provider is required. Filesystem and
         * allocation failures propagate; PNG images remain outside the untyped registry.
         */
        CE_API void register_files() const;
        /** Observe the last upload attempt, including partial publication.
         * The snapshot is synchronized, but provides no upload/rollback ordering
         * guarantee beyond the loading-owner contract.
         * @return Copied counters and completion status for the last attempt.
         */
        [[nodiscard]] CE_API UploadStats diagnostics() const;
        /** @return Owned snapshot of the last successfully submitted asset definitions.
         * Readers retain it across later uploads and loader destruction. This is
         * metadata, not an atomic snapshot of the global caches.
         */
        [[nodiscard]] std::shared_ptr<const std::vector<CE::Assets::Manifests::Graphics>> manifests() const {
            return manifests_.load(std::memory_order_acquire);
        }
        /** @return Owned snapshot of the last successfully submitted shader definitions,
         * with the same lifetime and cache-observation limits as manifests().
         */
        [[nodiscard]] std::shared_ptr<const std::vector<CE::Assets::Shaders::Manifest>> shader_manifests() const {
            return shaders_.load(std::memory_order_acquire);
        }
        /** Configure or retrieve the legacy singleton on the loading owner before producers start.
         * A different normalized root is rejected. Prefer owned Loader instances
         * for separate roots/lifetimes; they still share the supported cache domain.
         * @param root_path Root to initialize, or match against the existing singleton.
         * @return Borrowed process-lifetime loader with that root.
         */
        CE_API static Loader& get(const std::filesystem::path& root_path);
        /** Retrieve the configured legacy singleton without initializing it.
         * @return Borrowed process-lifetime loader; fails if initialization is unpublished.
         */
        CE_API static Loader& get();
    };
}

namespace CE::State {
    template <> CE_API auto Singleton<CE::Assets::Loading::Loader>::storage() -> Storage&;
}
