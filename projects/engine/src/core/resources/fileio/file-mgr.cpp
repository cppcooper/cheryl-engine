#include <core/resources/fileio/file-mgr.h>
#include <internals.h>
#include <algorithm>
#include <string>
#include <cctype>

namespace CE::Files {

    inline bool dir_exists(const ::std::filesystem::path& path) {
        return ::std::filesystem::exists(path) && ::std::filesystem::is_directory(path);
    }

    FileIndex::FileIndex(const ::std::filesystem::path& root_path) {
        search_directory(root_path);
    }

    void FileIndex::search_directory(const ::std::filesystem::path& directory) {
        if (dir_exists(directory) && !directories.contains(directory)) {
            directories.emplace(directory);
            ::std::filesystem::recursive_directory_iterator recursive_iter(directory);
            // One recursive walk supplies every extension bucket to asset loading.
            for (auto& entry : recursive_iter) {
                const ::std::filesystem::path& p = entry.path().filename();
                if (entry.is_regular_file()) {
                    Extension extension = p.extension().string();
                    if (!extension.empty()) {
                        std::ranges::transform(std::as_const(extension), extension.begin(), [](unsigned char c) {
                            return std::tolower(c);
                        });
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

    const std::vector<Path>& FileIndex::get_files_of_type(Extension extension) {
        // Normalize lookup keys the same way as scan-time extensions; the map
        // supplies an empty vector for types never encountered in this tree.
        std::transform(extension.cbegin(), extension.cend(), extension.begin(), [](unsigned char c) { return std::tolower(c); });
        return mapped_files[extension];
    }

}
