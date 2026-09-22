#include <core/resources/asset-management/font-mgr.h>

#include <assets/2d/stbfont.h>
#include <assets/abstracts/resource-provider.h>

#include <memory>

namespace CE::Assets {
    void FontMgr::load_assets(const std::vector<std::filesystem::path>& files, ResourceProvider& provider) {
        constexpr int default_font_size = 32;
        for (const auto& requested_file : files) {
            const auto file = requested_file.lexically_normal();
            if (loaded_assets.contains(file))
                continue;
            loaded_assets[file] = std::make_shared<STBFont>(STBFont::load_font(file, default_font_size, provider));
            if (default_font_path_.empty())
                default_font_path_ = file;
        }
    }

    FontMgr::spointer FontMgr::default_font() const {
        return default_font_path_.empty() ? nullptr : get_asset(default_font_path_);
    }
}
