#pragma once

#include <cheryl/export.h>
#include <templates/asset-mgr.h>
#include <templates/singleton.h>
#include <assets/types/2d/font.h>
#include <filesystem>
#include <vector>

namespace CE::Text::Legacy {
    /** Shared legacy-font cache operations; handles retain a Font independently of cache ownership. */
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

        CE_API void load(const std::vector<std::filesystem::path>& files, CE::Rendering::Resources::Provider& provider);

        [[nodiscard]] CE_API Handle default_font() const;
        CE_API void clear_assets() noexcept override;

    private:
        std::filesystem::path default_font_path_;
        CE_API void clear_metadata() noexcept override;
    };
}

namespace CE::State {
    template <> CE_API auto Singleton<CE::Text::Legacy::Manager>::storage() -> Storage&;
}

/** @fn CE::Text::Legacy::Manager::Manager()
 * Create an empty cache with no default; singleton initialize/get controls publication. The shared active provider/loading-owner domain remains global across managers.
 */

/** @fn CE::Text::Legacy::Manager::~Manager()
 * Release cache ownership; retained font handles remain valid within their backend lifetime.
 */

/** @fn void CE::Text::Legacy::Manager::load(const std::vector<std::filesystem::path>& files, CE::Rendering::Resources::Provider& provider)
 * Load supplied paths in order on the active provider loading owner/current context. Lexically normalized cached paths are reused; new fonts bake ASCII at 32pixels, collection face 0. The first successful publication becomes default. Errors propagate without rolling back earlier publications or trying another discovered font.
 * @see [Font discovery](cheryl-guide:assets/file-and-font-discovery)
 */

/** @fn CE::Text::Legacy::Manager::Handle CE::Text::Legacy::Manager::default_font() const
 * Return a retained shared default, null before loading or after clearing. Shared lookup does not create a font or perform I/O.
 */

/** @fn void CE::Text::Legacy::Manager::clear_assets() noexcept
 * Clear cache/default ownership while existing handles remain retained. Does not release/replace the shared provider/loading-owner domain.
 */

/** @fn template <> auto CE::State::Singleton<CE::Text::Legacy::Manager>::storage() -> Storage&
 * Return process-local singleton storage for the legacy font cache; use initialize/get for normal publication.
 */
