#include <core/resources/asset-management/font-mgr.h>

#include <assets/resources/resource-provider.h>
#include <assets/types/2d/stbfont.h>

#include <memory>
#include <utility>

namespace CE::Assets {
    void FontMgr::load_assets(
        const std::vector<std::filesystem::path>& files,
        ResourceProvider& provider
    ) {
        bind_provider(provider);
        constexpr int default_font_size = 32;
        for (const auto& requested_file : files) {
            const auto file = requested_file.lexically_normal();
            if (contains(file))
                continue;
            // Prepare the potentially allocating path copy before publication.
            auto default_path = file;
            auto font = std::make_shared<STBFont>(STBFont::load_font(file, default_font_size, provider));
            // The shared publication helper pins the candidate until its lock unwinds,
            // including node/rehash failure. Default selection commits without allocation.
            publish_asset(file, std::move(font), [&]() noexcept {
                if (default_font_path_.empty())
                    default_font_path_ = std::move(default_path);
            });
        }
    }

    FontMgr::spointer FontMgr::default_font() const {
        std::shared_lock lock(assets_mutex_);
        const auto entry = loaded_assets.find(default_font_path_);
        return entry == loaded_assets.end() ? nullptr : entry->second;
    }

    void FontMgr::clear_assets() noexcept {
        decltype(loaded_assets) retired;
        {
            std::unique_lock lock(assets_mutex_);
            retired.swap(loaded_assets);
            default_font_path_.clear();
        }
    }
}
