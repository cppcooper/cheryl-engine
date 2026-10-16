#pragma once

#include <cheryl/export.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace CE::Text {
    namespace Detail {
        struct FontCollectionData;
        struct FontAccess;
    }

    /** An explicit font file and zero-based collection face index; missing files are skipped during loading. */
    struct FontFile {
        std::filesystem::path path;
        std::uint32_t face_index{};
    };
    /** A system-only family request, bypassing application assets; missing families are skipped. */
    struct SystemFontFamily {
        std::string name;
    };
    /** Plain strings resolve family names or asset filenames, preferring application assets over system fonts.
     * FontFile selects a path/face directly; SystemFontFamily restricts a name to installed fonts.
     */
    using FontSource = std::variant<FontFile, SystemFontFamily, std::string>;

    /** Ordered font names and optional explicit sources, with application and installed-font discovery. */
    struct FontSelection {
        std::vector<::CE::Text::FontSource> preferred; ///< Names or explicit sources in application order; missing entries are skipped.
        bool automatic_system_fonts = true;            ///< Append automatic family choices, preferring application assets.
        /** Unset resolves system roots automatically; an empty vector disables installed-font discovery. */
        std::optional<std::vector<std::filesystem::path>> system_directories;
        /** Recursively search these application roots for family names and filename aliases (including extensions).
         * Names compare ASCII letters without case; an application match wins over an installed match.
         * Defaults to assets relative to the working directory; supply the game's asset root when different.
         * An empty vector disables application discovery. FontFile paths are independent of these roots.
         */
        std::vector<std::filesystem::path> asset_directories{"assets"};
    };
    struct FontFaceInfo {
        std::string family;
        std::string style;
        std::optional<std::filesystem::path> path;
        std::uint32_t face_index{};
        std::uint32_t glyph_count{};
        bool builtin{};
    };

    /** Immutable font bytes and ordered scalable Unicode faces. CPU-only loading
     * resolves preferred names from application assets before installed fonts, snapshots
     * the selected bytes, then appends an embedded DejaVu Sans fallback. Missing names
     * and files are skipped; an unreadable/unsupported explicit file fails the whole
     * load. No provider is used.
     */
    class FontCollection {
        std::shared_ptr<const Detail::FontCollectionData> data_;
        CE_API explicit FontCollection(std::shared_ptr<const Detail::FontCollectionData> data);
        friend struct Detail::FontAccess;

    public:
        /** Load an immutable font-byte snapshot without a provider or graphics context.
         * Invalid names and unreadable/unsupported explicit files reject the load;
         * missing names/files and unusable discovery candidates are skipped. Allocation failures propagate.
         * @param selection Ordered font names/sources, application roots and optional system-discovery policy,
         * borrowed only for this call.
         * @return Owned ordered faces ending in the embedded fallback, shared by copies.
         */
        [[nodiscard]] CE_API static FontCollection load(const ::CE::Text::FontSelection& selection = {});
        /** @return Borrowed ordered face metadata, empty after moving from the collection.
         * Keep the retained collection data alive while using the view; this call
         * performs no discovery or loading.
         */
        [[nodiscard]] CE_API std::span<const ::CE::Text::FontFaceInfo> faces() const noexcept;
        /** Test character-map coverage across the retained faces without a provider.
         * This does not select a font for a whole grapheme or shape text. A moved-from
         * collection or failure opening a retained face can reject a valid-scalar query.
         * @param scalar Unicode scalar value; surrogates/out-of-range values return false.
         * @return Whether at least one face maps the scalar to a glyph.
         */
        [[nodiscard]] CE_API bool covers(char32_t scalar) const;
    };
}
