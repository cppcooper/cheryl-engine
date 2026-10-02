#pragma once
#include <assets/definitions/manifest.h>
#include <assets/resources/decoded-image.h>
#include <templates/singleton.h>

#include <atomic>
#include <memory>
#include <vector>

namespace CE::Assets {
    struct ResourceProvider;
    struct PreparedImage {
        std::filesystem::path key;
        DecodedImage pixels;
    };
    struct PreparedAssets {
        std::vector<AssetManifest> manifests;
        std::vector<PreparedImage> images;
    };

    // An owned loader has one immutable root. Preparation uses no cache or provider;
    // upload obeys provider affinity and publishes a retained metadata snapshot.
    struct Loader final : Singleton_CTS<Loader> {
        explicit Loader(const std::filesystem::path& root_path) : root_path_(root_path.lexically_normal()) {}
        [[nodiscard]] PreparedAssets prepare() const;
        void upload(PreparedAssets prepared, ResourceProvider& provider);
        void load_assets(ResourceProvider& provider);
        [[nodiscard]] std::shared_ptr<const std::vector<AssetManifest>> manifests() const {
            return manifests_.load(std::memory_order_acquire);
        }
        // Legacy singleton access checks its root instead of silently reusing another root.
        static Loader& get(const std::filesystem::path& root_path);
        static Loader& get();

    private:
        const std::filesystem::path root_path_;
        std::atomic<std::shared_ptr<const std::vector<AssetManifest>>> manifests_{std::make_shared<const std::vector<AssetManifest>>()};
    };
}
