#pragma once
#include <assets/definitions/manifest.h>
#include <assets/resources/decoded-image.h>
#include <templates/singleton.h>
#include <core/diagnostics.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace CE::Assets {
    struct ResourceProvider;
    // Owns decoded staging pixels. No provider/native/cache handle is retained.
    struct PreparedImage {
        std::filesystem::path key;
        DecodedImage pixels;
    };
    struct PreparedAssets {
        std::vector<AssetManifest> manifests;
        std::vector<PreparedImage> images;
        Diagnostics::DomainId batch = Diagnostics::next_domain_id();
    };

    struct UploadStats {
        Diagnostics::DomainId batch = 0;
        Diagnostics::DomainId provider = 0;
        std::uint64_t images_completed = 0;
        std::uint64_t manifests_completed = 0;
        std::uint64_t publications = 0;
        bool publication_count_available = true;
        bool completed = false;
    };

    /** One immutable root; owned loaders can share the one supported cache domain.
     * CPU preparation changes no caches and can run on workers. Upload/publication
     * must be serialized on the active provider's loading owner. Relative roots
     * are interpreted against the working directory at preparation time.
     * This loader does not create a separate provider domain or atomic hot reload.
     */
    struct Loader final : Singleton_CTS<Loader> {
    private:
        const std::filesystem::path root_path_;
        std::atomic<std::shared_ptr<const std::vector<AssetManifest>>> manifests_{std::make_shared<const std::vector<AssetManifest>>()};
        mutable std::mutex observations_mutex_;
        UploadStats last_upload_;

    public:
        explicit Loader(const std::filesystem::path& root_path)
        : root_path_(root_path.lexically_normal()) {}
        // Rescans/decodes owned data and validates its definitions before upload.
        // Failure leaves both caches and the retained metadata snapshot unchanged.
        [[nodiscard]] PreparedAssets prepare() const;
        /** Consumes one prepared value without reopening its image files. Existing
         * keys remain; each successful new entry can become visible immediately.
         * Failure can leave any completed cache entries, including all entries if
         * final metadata allocation fails. manifests() retains the previous snapshot.
         * Retry by preparing again or by keeping a copy before moving the input;
         * a consumed failed value is not returned. Retry preserves existing keys,
         * not replacement of their contents. Pending platform requests may cancel
         * before execution; once executing, upload has no rollback/cancellation API.
         */
        void upload(PreparedAssets prepared, ResourceProvider& provider);
        void load_assets(ResourceProvider& provider);
        // Last upload attempt, including partial publication. Observation only;
        // no upload/rollback ordering guarantee beyond the loading-owner contract.
        [[nodiscard]] UploadStats diagnostics() const;
        // Last successfully submitted definitions, not an atomic snapshot of the
        // global caches. Readers retain this metadata after later upload/destruction.
        [[nodiscard]] std::shared_ptr<const std::vector<AssetManifest>> manifests() const {
            return manifests_.load(std::memory_order_acquire);
        }
        // Legacy singleton access checks its root instead of silently reusing another root.
        // Configure on the loading owner before producers start; prefer owned Loader instances
        // when separate roots/lifetimes are needed. get() rejects unpublished initialization.
        static Loader& get(const std::filesystem::path& root_path);
        static Loader& get();

    };
}
