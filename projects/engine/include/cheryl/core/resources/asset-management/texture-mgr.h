#pragma once

#include <cheryl/export.h>
#include <assets/resources/decoded-image.h>
#include <assets/resources/image.h>
#include <templates/asset-mgr.h>
#include <templates/singleton.h>

namespace CE::Rendering::Resources {
    struct Provider;
}

namespace CE::Assets::Images {
    /** Strong immutable image cache keyed by filesystem paths. */
    using Store = CE::Assets::Cache::Store<CE::Rendering::Resources::Image>;

    /** Image cache with exact normalized-path lookup and optional unambiguous basename fallback.
     * Publication is serialized on the global provider/loading owner; reads copy
     * retained handles under the cache lock. Existing image contents are immutable.
     * @see [Resource residency](cheryl-guide:resources/resource-residency)
     */
    struct CE_TYPE_API Manager final : Store,
                                       CE::State::Singleton<Manager> {
        Manager() = default;
        ~Manager() override = default;
        // Exact lexically normalized path first; a bare filename may uniquely match
        // another path. Missing returns null, ambiguity throws. contains() is exact-key only.
        [[nodiscard]] CE_API Handle get_asset(const std::filesystem::path& file) const override;
        // Serialize on the cache/provider loading owner. Existing normalized keys
        // are reused without rereading/replacing their immutable image contents.
        // Batch failure can leave earlier entries; input pixels are not retained.
        CE_API void load(const std::vector<std::filesystem::path>& files, CE::Rendering::Resources::Provider& provider);
        CE_API void upload(const std::filesystem::path& file, const Decoded& image, CE::Rendering::Resources::Provider& provider);
    };
}

namespace CE::State {
    template <> CE_API auto Singleton<CE::Assets::Images::Manager>::storage() -> Storage&;
}

/**
 * @fn CE::Assets::Images::Manager::Manager()
 * @brief Create an empty image cache sharing the supported provider/loading-owner policy.
 */

/**
 * @fn CE::Assets::Images::Manager::~Manager()
 * @brief Drop image-cache strong handles; external readers/frames keep independent image ownership.
 */

/**
 * @fn CE::Assets::Images::Manager::Handle CE::Assets::Images::Manager::get_asset(const std::filesystem::path& file) const
 * @brief Look up a lexically normalized exact path first, then a bare filename only if unique.
 * @param file Exact path, or basename without parent components for convenience fallback.
 * @return Retained immutable image or null if absent. Multiple basename matches throw runtime_exception.
 * Lookup is synchronized; inherited contains() remains exact-key-only. Relative paths are not made absolute by this method.
 */

/**
 * @fn void CE::Assets::Images::Manager::load(const std::vector<std::filesystem::path>& files, CE::Rendering::Resources::Provider& provider)
 * @brief Decode/upload absent normalized keys on the active provider loading owner/current context.
 * @param files Paths borrowed through synchronous load; relative paths follow provider file I/O working-directory rules.
 * @param provider Single active backend domain.
 * Existing entries skip file reads/upload. Decode/backend/allocation failure can leave earlier entries published; no batch replacement or rollback occurs.
 */

/**
 * @fn void CE::Assets::Images::Manager::upload(const std::filesystem::path& file, const Decoded& image, CE::Rendering::Resources::Provider& provider)
 * @brief Publish an absent normalized image key from already decoded CPU pixels.
 * @param file Intended exact path key, lexically normalized without reopening it.
 * @param image Pixels borrowed through create_image(); caller retains/reuses them after return.
 * @param provider Active backend loading owner/current context.
 * Existing keys skip upload. Invalid/native/allocation failure publishes no candidate for this key; earlier cache entries remain.
 * @see [Staged loading](cheryl-guide:assets/asset-loading)
 */
