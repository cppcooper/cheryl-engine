#pragma once

#include <cheryl/export.h>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace CE::Files {

    /** Extension key including the leading dot; extensionless files use file_no_ext. */
    using Extension = std::string;
    /** Owned filesystem path, not an open file or canonical physical identity. */
    using Path = std::filesystem::path;

    /** Incremental file index by extension beneath explicitly supplied directories.
 * There is no refresh/removal or internal synchronization. Construct a new
 * manager to rescan a previously indexed root.
 */
    class FileIndex {
    public:
        CE_API FileIndex(const std::filesystem::path& root_path);

        CE_API void search_directory(const std::filesystem::path& directory);

        CE_API const std::vector<::CE::Files::Path>& get_files_of_type(::CE::Files::Extension extension);

        // Borrow all extension buckets without inserting missing keys. Serialize
        // access with directory indexing; the manager owns the returned index.
        [[nodiscard]] const std::unordered_map<::CE::Files::Extension, std::vector<::CE::Files::Path>>& files_by_type() const noexcept {
            return mapped_files;
        }

    private:
        std::unordered_set<std::filesystem::path> directories;
        std::unordered_map<::CE::Files::Extension, std::vector<::CE::Files::Path>> mapped_files;
    };

}

/**
 * @fn CE::Files::FileIndex::FileIndex(const std::filesystem::path& root_path)
 * Scan root_path using search_directory(); missing/non-directory roots add nothing.
 */

/**
 * @fn void CE::Files::FileIndex::search_directory(const std::filesystem::path& directory)
 * Append regular files beneath an unvisited directory and mark encountered
 * directories as visited. Keys use path equality without canonicalization;
 * alternate paths can index the same physical file more than once. Extensions
 * are lowercased with their leading dot; extensionless files use "file_no_ext".
 * Enumeration is unsorted. Filesystem/allocation errors propagate and can leave
 * a partial index with the root marked visited; recreate the index to retry.
 */

/**
 * @fn const std::vector<CE::Files::Path>& CE::Files::FileIndex::get_files_of_type(CE::Files::Extension extension)
 * Borrow the manager-owned vector for a lowercased extension key. A missing
 * key inserts an empty bucket; use ".png", not "png", or "file_no_ext".
 * Reborrow after assigning/moving the manager or replacing its index; destruction
 * invalidates the reference. Later directory indexing can change the vector's
 * contents and invalidate its element references/iterators.
 * Serialize this lookup with all other access; insertion can allocate/throw.
 */

/**
 * @fn auto CE::Files::FileIndex::files_by_type() const noexcept -> const std::unordered_map<CE::Files::Extension, std::vector<CE::Files::Path>>&
 * @return Borrowed extension buckets without inserting a missing key. Retain this index and serialize access with scans/assignment/moves; further indexing may change bucket contents and invalidate element references/iterators.
 * @see [File indexing](cheryl-guide:assets/file-and-font-discovery#file-index)
 */
