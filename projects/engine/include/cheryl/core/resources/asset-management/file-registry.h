#pragma once

#include <cheryl/export.h>

#include <filesystem>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <templates/singleton.h>

namespace CE::Assets::Loading {
    /** Synchronized path registry for files without typed resource loading.
     * Paths are owned, normalized absolute names grouped by basename; contents
     * remain the responsibility of the consumer opening/streaming them.
     * @see [File discovery](cheryl-guide:assets/file-and-font-discovery#file-index)
     */
    // Lightweight registry for asset files that do not need a typed manager.
    // It stores absolute paths grouped by basename; file contents remain owned
    // by the consumer that opens or streams them.
    class FileRegistry final : public CE::State::Singleton<FileRegistry> {
        std::unordered_map<std::string, std::vector<std::filesystem::path>> files_;
        mutable std::shared_mutex mutex_;

    public:
        FileRegistry() = default;

        // Append files without replacing earlier registrations. Paths become
        // lexically normalized absolute paths; identical paths are deduplicated.
        // Allocation failure can leave completed insertions registered.
        CE_API void register_files(const std::vector<std::filesystem::path>& files);

        // Append a prepared registry using the same path-deduplication policy.
        CE_API void register_files(const FileRegistry& files);

        // Returns an owned, sorted snapshot for this exact basename. Later
        // registration cannot invalidate it; missing names return an empty vector.
        [[nodiscard]] CE_API std::vector<std::filesystem::path> get_files_named(std::string_view name) const;

        // Returns the lexically first matching absolute path, if any.
        [[nodiscard]] CE_API std::optional<std::filesystem::path> get_file_named(std::string_view name) const;

        // Resolve an exact registered path, not the first matching basename.
        // Relative input is interpreted against the current working directory.
        [[nodiscard]] CE_API std::optional<std::filesystem::path> get_file_at(const std::filesystem::path& path) const;

    private:
        CE_API void insert_file(const std::filesystem::path& path);
    };
}

namespace CE::State {
    template <> CE_API auto Singleton<CE::Assets::Loading::FileRegistry>::storage() -> Storage&;
}

/**
 * @fn CE::Assets::Loading::FileRegistry::FileRegistry()
 * @brief Create an empty owned registry without scanning files or touching its optional legacy singleton.
 */

/**
 * @fn void CE::Assets::Loading::FileRegistry::register_files(const std::vector<std::filesystem::path>& files)
 * @brief Append normalized absolute paths under the registry write lock, deduplicating identical paths.
 * @param files Paths borrowed through the call; relative paths use the working directory at registration.
 * No files are opened. Filesystem normalization/allocation errors propagate; completed insertions may remain. Existing registrations are never replaced.
 */

/**
 * @fn void CE::Assets::Loading::FileRegistry::register_files(const FileRegistry& files)
 * @brief Merge another registry under coordinated destination-write/source-read locks.
 * @param files Live registry borrowed through the call; self-registration is a no-op.
 * Copies and deduplicates paths without opening contents. Allocation failure can leave completed insertions; source remains unchanged.
 */

/**
 * @fn std::vector<std::filesystem::path> CE::Assets::Loading::FileRegistry::get_files_named(std::string_view name) const
 * @brief Copy the sorted path bucket for an exact basename under a shared lock.
 * @param name Exact case-sensitive basename, not a path query.
 * @return Independent lexically sorted paths, empty if absent; later registration/destruction cannot invalidate the copy.
 */

/**
 * @fn std::optional<std::filesystem::path> CE::Assets::Loading::FileRegistry::get_file_named(std::string_view name) const
 * @brief Copy the lexically first path for an exact basename under a shared lock.
 * @param name Exact basename; multiple matches do not report ambiguity here.
 * @return Owned first path, or nullopt if absent. Use get_files_named() when selection policy matters.
 */

/**
 * @fn std::optional<std::filesystem::path> CE::Assets::Loading::FileRegistry::get_file_at(const std::filesystem::path& path) const
 * @brief Resolve an exact registered normalized absolute path, without basename fallback.
 * @param path Relative input uses the current working directory; no filesystem canonicalization or existence check is added.
 * @return Owned matching path, or nullopt when absent. Filesystem/allocation failures propagate; registry access is synchronized.
 */
