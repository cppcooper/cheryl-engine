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
        std::filesystem::path path; ///< Explicit path, relative to the process working directory unless absolute.
        std::uint32_t face_index{}; ///< Zero-based face in a font collection; zero for ordinary single-face files.
    };
    /** A system-only family request, bypassing application assets; missing families are skipped. */
    struct SystemFontFamily {
        std::string name; ///< Installed family name, matched without ASCII case; cannot be empty or contain NUL.
    };
    /** Plain strings resolve family names or asset filenames, preferring application assets over system fonts.
     * FontFile selects a path/face directly; SystemFontFamily restricts a name to installed fonts.
     */
    using FontSource = std::variant<FontFile, SystemFontFamily, std::string>;

    /** Ordered font names and optional explicit sources, with application and installed-font discovery. */
    struct FontSelection {
        std::vector<::CE::Text::FontSource> preferred; ///< Names or explicit sources in application order; missing entries are skipped.
        bool automatic_system_fonts = true;            ///< Append automatic family choices, preferring application assets.
        std::optional<std::vector<std::filesystem::path>> system_directories; ///< Unset uses system roots; empty disables installed discovery.
        std::vector<std::filesystem::path> asset_directories{"assets"}; ///< Recursive application discovery roots; empty disables it.
        bool include_family_styles = false; ///< Retain style variants for catalogue registration instead of one best face per family.
    };
    /** Owned metadata describing a face in a retained FontCollection; paths identify origin, not a live file dependency. */
    struct FontFaceInfo {
        std::string family; ///< Native family name from the selected face.
        std::string style; ///< Native style name; selection also uses numeric weight and italic metadata.
        std::optional<std::filesystem::path> path; ///< Source path when file-backed; absent for the embedded fallback.
        std::uint32_t face_index{}; ///< Zero-based face within the source file/collection.
        std::uint32_t glyph_count{}; ///< Declared glyph count, not Unicode coverage size.
        bool builtin{}; ///< True for the embedded safety face.
        std::uint16_t weight = 400; ///< Declared OpenType weight, or regular/bold fallback metadata.
        bool italic{}; ///< Declared italic style.
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
        friend class Font;

    public:
        [[nodiscard]] CE_API static FontCollection load(const ::CE::Text::FontSelection& selection = {});
        [[nodiscard]] CE_API std::span<const ::CE::Text::FontFaceInfo> faces() const noexcept;
        [[nodiscard]] CE_API bool covers(char32_t scalar) const;
    };
}

/** @fn CE::Text::FontCollection CE::Text::FontCollection::load(const FontSelection& selection)
 * Load an owned immutable CPU snapshot, ending in embedded DejaVu Sans.
 * Missing names/files and unusable discovery candidates are skipped; invalid names and unreadable/unsupported explicit files reject the load. Allocation failures propagate. No provider/graphics context is used.
 * @param selection Ordered names/sources and discovery roots, borrowed for this call.
 * @return Shared immutable ordered faces; copies retain the same bytes.
 * @see [Font selection](cheryl-guide:assets/text-layout#font-selection)
 */

/** @fn std::span<const CE::Text::FontFaceInfo> CE::Text::FontCollection::faces() const noexcept
 * Borrow ordered metadata; empty after move. Keep retained collection data alive while using the view. No discovery or loading occurs.
 */

/** @fn bool CE::Text::FontCollection::covers(char32_t scalar) const
 * Test character-map coverage across retained faces. Surrogates/out-of-range values return false; a moved-from collection or face-opening failure may reject valid queries. No whole-grapheme selection or shaping is performed.
 */

/** @var std::vector<std::filesystem::path> CE::Text::FontSelection::asset_directories
 * Family names and filenames including extension compare without ASCII case. An application match wins over installed fonts. Defaults are relative to the working directory; explicit FontFile paths are independent of these roots.
 */
