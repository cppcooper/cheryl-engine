#pragma once
#include <assets/manifest.h>
#include <templates/singleton.h>
#include <core/resources/fileio/file-mgr.h>

#include <vector>

namespace CE::Assets {
    struct ResourceProvider;

    // TODO: Singleton_CTS permanently captures the first root_path passed to Loader::get(). Revisit
    // loader ownership before supporting multiple asset roots, isolated tests with different roots,
    // or switching/hot-reloading the root within one process.
    struct Loader : Singleton_CTS<Loader>, FileMgr {
        explicit Loader(const std::filesystem::path& root_path) :
            FileMgr(root_path), root_path_(root_path.lexically_normal()) {}
        void load_assets(ResourceProvider& provider);
        [[nodiscard]] const std::vector<AssetManifest>& manifests() const { return manifests_; }

    private:
        std::filesystem::path root_path_;
        std::vector<AssetManifest> manifests_;
    };
}
