#include <core/resources/fileio/file-mgr.h>
#include <internals.h>
#include <algorithm>
#include <string>
#include <cctype>

inline bool dir_exists(const fs::path& path) {
    return fs::exists(path) && fs::is_directory(path);
}

FileMgr::FileMgr(const fs::path& root_path) {
    search_directory(root_path);
}

void FileMgr::search_directory(const fs::path& directory) {
    if (dir_exists(directory) && !directories.contains(directory)) {
        directories.emplace(directory);
        fs::recursive_directory_iterator recursive_iter(directory);
        // Walk nested directories once and index files by normalized extension
        // for later shader/texture discovery by the asset loader.
        for (auto& entry : recursive_iter) {
            const fs::path& p = entry.path().filename();
            if (entry.is_regular_file()) {
                f_ext extension = p.extension().string();
                if (!extension.empty()) {
                    std::ranges::transform(std::as_const(extension), extension.begin(),
                                           [](unsigned char c) { return std::tolower(c); });
                    mapped_files[extension].push_back(entry.path());
                } else {
                    mapped_files["file_no_ext"].push_back(entry.path());
                }
            } else if (entry.is_directory()) {
                directories.emplace(entry.path());
            }
        }
    }
}

const std::vector<fspath>& FileMgr::get_files_of_type(f_ext extension) {
    // Normalize lookup keys the same way as scan-time extensions; the map
    // supplies an empty vector for types never encountered in this tree.
    std::transform(extension.cbegin(), extension.cend(), extension.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return mapped_files[extension];
}
