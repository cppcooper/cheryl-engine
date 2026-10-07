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

    struct FontFile {
        std::filesystem::path path;
        std::uint32_t face_index{};
    };
    struct SystemFontFamily {
        std::string name;
    };
    using FontSource = std::variant<FontFile, SystemFontFamily>;

    struct FontSelection {
        std::vector<FontSource> preferred; // Files/families in application order.
        bool automatic_system_fonts = true;
        // Unset uses the system roots; an empty vector disables discovery.
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
        [[nodiscard]] static FontCollection load(const FontSelection& selection = {});
        [[nodiscard]] std::span<const FontFaceInfo> faces() const noexcept;
        // Cmap coverage only; whole-grapheme selection/shaping belongs to layout.
        [[nodiscard]] bool covers(char32_t scalar) const;
    };
}
