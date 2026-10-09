#pragma once
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using f_ext = std::string;
using fspath = std::filesystem::path;

/** Incremental file index by extension beneath explicitly supplied directories.
 * There is no refresh/removal or internal synchronization. Construct a new
 * manager to rescan a previously indexed root.
 */
class FileMgr {
public:
    /** Scan root_path using search_directory(); missing/non-directory roots add nothing. */
    FileMgr(const std::filesystem::path& root_path);

    /** Append regular files beneath an unvisited directory and mark encountered
     * directories as visited. Keys use path equality without canonicalization;
     * alternate paths can index the same physical file more than once. Extensions
     * are lowercased with their leading dot; extensionless files use "file_no_ext".
     * Enumeration is unsorted. Filesystem/allocation errors propagate and can leave
     * a partial index with the root marked visited; recreate the index to retry.
     */
    void search_directory(const std::filesystem::path& directory);

    /** Borrow the manager-owned vector for a lowercased extension key. A missing
     * key inserts an empty bucket; use ".png", not "png", or "file_no_ext".
     * Reborrow after assigning/moving the manager or replacing its index; destruction
     * invalidates the reference. Later directory indexing can change the vector's
     * contents and invalidate its element references/iterators.
     * Serialize this lookup with all other access; insertion can allocate/throw.
     */
    const std::vector<fspath>& get_files_of_type(f_ext extension);

    // Borrow all extension buckets without inserting missing keys. Serialize
    // access with directory indexing; the manager owns the returned index.
    [[nodiscard]] const std::unordered_map<f_ext, std::vector<fspath>>& files_by_type() const noexcept { return mapped_files; }

private:
    std::unordered_set<std::filesystem::path> directories;
    std::unordered_map<f_ext, std::vector<fspath>> mapped_files;
};
