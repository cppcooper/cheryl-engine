#pragma once

#include <cheryl/export.h>
#include <templates/asset-mgr.h>
#include <templates/singleton.h>
#include <assets/types/2d/font.h>
#include <filesystem>
#include <vector>

namespace CE::Text::Legacy {
    using Store = CE::Assets::Cache::Store<CE::Text::Legacy::Font>;
}

namespace CE::Rendering::Resources {
    struct Provider;
}

namespace CE::Text::Legacy {
    /** Path-keyed font cache in the shared provider/loading-owner domain.
     * Loading uses STB's printable-ASCII atlas and collection face zero.
     * System-font discovery/default candidate selection are separate utilities.
     */
    struct CE_TYPE_API Manager final : Store,
                                       CE::State::Singleton<Manager> {
        Manager() = default;
        ~Manager() override = default;

        /** Load in supplied order on the active provider's loading thread. Keys
         * are lexically normalized paths; cached entries are retained. Newly loaded
         * fonts use size 32 and select collection face zero. The first successful
         * publication becomes the default if none exists. Errors propagate without
         * rolling back earlier publications or trying an alternate system font.
         */
        CE_API void load(const std::vector<std::filesystem::path>& files, CE::Rendering::Resources::Provider& provider);

        /** Retain the published default, or return nullptr before loading/after clear. */
        [[nodiscard]] CE_API Handle default_font() const;

        /** Clear cache/default ownership; existing shared handles remain valid.
         * This does not release or replace the shared provider/loading-owner domain.
         */
        CE_API void clear_assets() noexcept override;

    private:
        std::filesystem::path default_font_path_;
    };
}

namespace CE::State {
    template <> CE_API auto Singleton<CE::Text::Legacy::Manager>::storage() -> Storage&;
}
