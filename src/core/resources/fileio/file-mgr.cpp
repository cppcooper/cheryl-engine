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
        // recursively iterate directory's content
        for (auto& entry : recursive_iter) {
            // get the path and filename ready
            const fs::path& p = entry.path().filename();
            // organize according to extension / directory / no extension
            if (entry.is_regular_file()) {
                f_ext extension = p.extension().string();
                if (!extension.empty()) {
                    // normal files
                    std::ranges::transform(std::as_const(extension), extension.begin(),
                                           [](unsigned char c) { return std::tolower(c); });
                    mapped_files[extension].push_back(entry.path());
                } else {
                    // no extension files
                    mapped_files["file_no_ext"].push_back(entry.path());
                }
            } else if (entry.is_directory()) {
                // directories
                directories.emplace(entry.path());
            }
        }
    }
}

const std::vector<fspath>& FileMgr::get_files_of_type(f_ext extension) {
    std::transform(extension.cbegin(), extension.cend(), extension.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return mapped_files[extension];
}
