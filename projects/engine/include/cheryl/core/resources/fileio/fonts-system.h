#pragma once
#include <filesystem>
#include <optional>
#include <vector>

namespace CE::Resources {
    /** Return owned candidate roots from built-in Unix/macOS paths and nonempty
     * HOME/XDG_DATA_HOME/WINDIR/LOCALAPPDATA values. No existence check or deduplication
     * occurs here. Keep environment mutation serialized with this call.
     */
    [[nodiscard]] std::vector<std::filesystem::path> system_font_directories();

    /** Discover fonts using system_font_directories() with the explicit-root overload's policy. */
    [[nodiscard]] std::vector<std::filesystem::path> find_system_fonts();

    /** Best-effort recursive discovery of regular .ttf/.otf/.ttc/.otc paths,
     * matched case-insensitively. Missing/non-directory/inaccessible roots and
     * permission-denied subtrees are skipped; other traversal errors can truncate
     * a root's results without diagnostics. Directory symlinks are not followed.
     * Return owned, lexically normalized, path-sorted/deduplicated candidates;
     * aliases are not canonicalized and font contents are not validated.
     * Filesystem errors use error_code; allocation/path-conversion failures propagate.
     */
    [[nodiscard]] std::vector<std::filesystem::path> find_system_fonts(const std::vector<std::filesystem::path>& directories);

    /** Choose a candidate by the filename preference order documented in
     * docs/assets/file-and-font-discovery.md. Filename matching ignores case;
     * equal preferred names use the supplied order. Without a preferred name,
     * return the minimum path; an empty list returns nullopt. No I/O, font-family
     * inspection or load-failure fallback is performed. The path is an owned copy.
     */
    [[nodiscard]] std::optional<std::filesystem::path> select_default_system_font(const std::vector<std::filesystem::path>& fonts);
}
