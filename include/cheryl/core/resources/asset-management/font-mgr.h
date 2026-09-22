#pragma once
#include <templates/asset-mgr.h>
#include <templates/singleton.h>
#include <assets/abstracts/font.h>
#include <filesystem>
#include <vector>

using FMgr = CE::Assets::AssetMgr<CE::Assets::Font>;
namespace CE::Assets {
    struct FontMgr final : FMgr, Singleton_CTS<FontMgr> {
        FontMgr() = default;
        ~FontMgr() override = default;
        void load_assets(const std::vector<std::filesystem::path>& files);
        [[nodiscard]] spointer default_font() const;

    private:
        std::filesystem::path default_font_path_;
    };
}
