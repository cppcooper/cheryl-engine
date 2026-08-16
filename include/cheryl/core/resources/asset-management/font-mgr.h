#pragma once
#include <templates/asset-mgr.h>
#include <templates/singleton.h>
#include <assets/abstracts/font.h>

using FMgr = CE::Assets::AssetMgr<CE::Assets::Font>;
namespace CE::Assets {
    struct FontMgr final : FMgr, Singleton_CTS<FontMgr> {
        FontMgr() = default;
        ~FontMgr() override = default;
        void load_assets(const std::vector<fs::path>&) override;
    };
}
