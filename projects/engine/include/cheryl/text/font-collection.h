#pragma once

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

    /** An explicit font file and zero-based collection face index. */
    struct FontFile {
        std::filesystem::path path;
        std::uint32_t face_index{};
    };
    /** An installed family requested by name; missing families are skipped during loading. */
    struct SystemFontFamily {
        std::string name;
    };
    using FontSource = std::variant<FontFile, SystemFontFamily>;

    /** Ordered application choices and optional installed-font discovery. */
    struct FontSelection {
        std::vector<FontSource> preferred; ///< Files/families in application order.
        bool automatic_system_fonts = true; ///< Append automatic installed-family choices.
        /** Unset uses system roots; an empty vector disables installed-font discovery. */
        std::optional<std::vector<std::filesystem::path>> system_directories;
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
     * snapshots application files and optional installed families, then appends an
     * embedded DejaVu Sans fallback. Missing installed families are skipped; an
     * unreadable/unsupported explicit file fails the whole load. No provider is used.
     */
    class FontCollection {
        std::shared_ptr<const Detail::FontCollectionData> data_;
        explicit FontCollection(std::shared_ptr<const Detail::FontCollectionData> data);
        friend struct Detail::FontAccess;

    public:
        /** Load an immutable font-byte snapshot without a provider or graphics context.
         * Invalid selections, explicit file failures and unsupported faces reject
         * the load; missing installed families are skipped. Allocation failures propagate.
         * @param selection Ordered application fonts and optional system-discovery policy,
         * borrowed only for this call.
         * @return Owned ordered faces ending in the embedded fallback, shared by copies.
         */
        [[nodiscard]] static FontCollection load(const FontSelection& selection = {});
        /** @return Borrowed ordered face metadata, empty after moving from the collection.
         * Keep the retained collection data alive while using the view; this call
         * performs no discovery or loading.
         */
        [[nodiscard]] std::span<const FontFaceInfo> faces() const noexcept;
        /** Test character-map coverage across the retained faces without a provider.
         * This does not select a font for a whole grapheme or shape text. A moved-from
         * collection or failure opening a retained face can reject a valid-scalar query.
         * @param scalar Unicode scalar value; surrogates/out-of-range values return false.
         * @return Whether at least one face maps the scalar to a glyph.
         */
        [[nodiscard]] bool covers(char32_t scalar) const;
    };
}
