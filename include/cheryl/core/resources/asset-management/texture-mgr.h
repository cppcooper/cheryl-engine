#pragma once
#include <templates/asset-mgr.h>
#include <templates/singleton.h>
#include <assets/abstracts/image.h>

namespace CE::Assets {
    struct ResourceProvider;
}

using TMgr = CE::Assets::AssetMgr<CE::Assets::Image>;
namespace CE::Assets {
    struct TextureMgr final : TMgr, Singleton_CTS<TextureMgr> {
        TextureMgr() = default;
        ~TextureMgr() override = default;
        [[nodiscard]] spointer get_asset(const std::filesystem::path& file) const override;
        void load_assets(const std::vector<std::filesystem::path>& files, ResourceProvider& provider);
    };
}
