#pragma once
#include <templates/asset-mgr.h>
#include <templates/singleton.h>
#include <assets/types/2d/font.h>
#include <filesystem>
#include <vector>

using FMgr = CE::Assets::AssetMgr<CE::Assets::Font>;
namespace CE::Assets {
    struct ResourceProvider;

    struct FontMgr final : FMgr, Singleton_CTS<FontMgr> {
        FontMgr() = default;
        ~FontMgr() override = default;
        void load_assets(const std::vector<std::filesystem::path>& files, ResourceProvider& provider);
        [[nodiscard]] spointer default_font() const;
        void clear_assets() noexcept override;

    private:
        std::filesystem::path default_font_path_;
    };
}
