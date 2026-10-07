#pragma once
#include <assets/resources/decoded-image.h>
#include <assets/resources/image.h>
#include <templates/asset-mgr.h>
#include <templates/singleton.h>

namespace CE::Assets {
    struct ResourceProvider;
}

using TMgr = CE::Assets::AssetMgr<CE::Assets::Image>;
namespace CE::Assets {
    struct TextureMgr final : TMgr,
                              Singleton_CTS<TextureMgr> {
        TextureMgr() = default;
        ~TextureMgr() override = default;
        // Exact lexically normalized path first; a bare filename may uniquely match
        // another path. Missing returns null, ambiguity throws. contains() is exact-key only.
        [[nodiscard]] spointer get_asset(const std::filesystem::path& file) const override;
        // Serialize on the cache/provider loading owner. Existing normalized keys
        // are reused without rereading/replacing their immutable image contents.
        // Batch failure can leave earlier entries; input pixels are not retained.
        void load_assets(const std::vector<std::filesystem::path>& files, ResourceProvider& provider);
        void load_asset(const std::filesystem::path& file, const DecodedImage& image, ResourceProvider& provider);
    };
}
