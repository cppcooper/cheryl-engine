#include <core/resources/asset-management/file-registry.h>

#include <algorithm>
#include <mutex>

namespace CE::Assets {
    void FileRegistry::register_files(const std::vector<std::filesystem::path>& files) {
        std::vector<std::filesystem::path> absolute_files;
        absolute_files.reserve(files.size());
        for (const auto& file : files)
            absolute_files.push_back(std::filesystem::absolute(file).lexically_normal());
        std::unique_lock lock(mutex_);
        for (const auto& file : absolute_files)
            insert_file(file);
    }

    void FileRegistry::register_files(const FileRegistry& files) {
        if (&files == this)
            return;
        std::unique_lock destination(mutex_, std::defer_lock);
        std::shared_lock source(files.mutex_, std::defer_lock);
        std::lock(destination, source);
        for (const auto& entry : files.files_)
            for (const auto& path : entry.second)
                insert_file(path);
    }

    void FileRegistry::insert_file(const std::filesystem::path& path) {
        auto& matches = files_[path.filename().string()];
        const auto position = std::ranges::lower_bound(matches, path);
        if (position == matches.end() || *position != path)
            matches.insert(position, path);
    }

    std::vector<std::filesystem::path> FileRegistry::get_files_named(const std::string_view name) const {
        std::shared_lock lock(mutex_);
        const auto found = files_.find(std::string{name});
        return found == files_.end() ? std::vector<std::filesystem::path>{} : found->second;
    }

    std::optional<std::filesystem::path> FileRegistry::get_file_named(const std::string_view name) const {
        std::shared_lock lock(mutex_);
        const auto found = files_.find(std::string{name});
        if (found == files_.end() || found->second.empty())
            return std::nullopt;
        return found->second.front();
    }

    std::optional<std::filesystem::path> FileRegistry::get_file_at(const std::filesystem::path& path) const {
        const auto absolute_path = std::filesystem::absolute(path).lexically_normal();
        std::shared_lock lock(mutex_);
        const auto found = files_.find(absolute_path.filename().string());
        if (found == files_.end())
            return std::nullopt;
        const auto position = std::ranges::lower_bound(found->second, absolute_path);
        if (position == found->second.end() || *position != absolute_path)
            return std::nullopt;
        return *position;
    }
}
