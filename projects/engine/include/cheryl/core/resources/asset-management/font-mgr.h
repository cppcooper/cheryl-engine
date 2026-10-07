#pragma once
#include <templates/asset-mgr.h>
#include <templates/singleton.h>
#include <assets/types/2d/font.h>
#include <filesystem>
#include <vector>

using FMgr = CE::Assets::AssetMgr<CE::Assets::Font>;
namespace CE::Assets {
    struct ResourceProvider;

    /** Path-keyed font cache in the shared provider/loading-owner domain.
     * Loading uses STB's printable-ASCII atlas and collection face zero.
     * System-font discovery/default candidate selection are separate utilities.
     */
    struct FontMgr final : FMgr,
                           Singleton_CTS<FontMgr> {
        FontMgr() = default;
        ~FontMgr() override = default;

        /** Load in supplied order on the active provider's loading thread. Keys
         * are lexically normalized paths; cached entries are retained. Newly loaded
         * fonts use size 32 and select collection face zero. The first successful
         * publication becomes the default if none exists. Errors propagate without
         * rolling back earlier publications or trying an alternate system font.
         */
        void load_assets(const std::vector<std::filesystem::path>& files, ResourceProvider& provider);

        /** Retain the published default, or return nullptr before loading/after clear. */
        [[nodiscard]] spointer default_font() const;

        /** Clear cache/default ownership; existing shared handles remain valid.
         * This does not release or replace the shared provider/loading-owner domain.
         */
        void clear_assets() noexcept override;

    private:
        std::filesystem::path default_font_path_;
    };
}
