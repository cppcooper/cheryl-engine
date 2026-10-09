#pragma once

#include <filesystem>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <templates/singleton.h>

namespace CE::Assets {
    // Lightweight registry for asset files that do not need a typed manager.
    // It stores absolute paths grouped by basename; file contents remain owned
    // by the consumer that opens or streams them.
    class FileRegistry final : public Singleton_CTS<FileRegistry> {
        std::unordered_map<std::string, std::vector<std::filesystem::path>> files_;
        mutable std::shared_mutex mutex_;

    public:
        FileRegistry() = default;

        // Append files without replacing earlier registrations. Paths become
        // lexically normalized absolute paths; identical paths are deduplicated.
        // Allocation failure can leave completed insertions registered.
        void register_files(const std::vector<std::filesystem::path>& files);

        // Append a prepared registry using the same path-deduplication policy.
        void register_files(const FileRegistry& files);

        // Returns an owned, sorted snapshot for this exact basename. Later
        // registration cannot invalidate it; missing names return an empty vector.
        [[nodiscard]] std::vector<std::filesystem::path> get_files_named(std::string_view name) const;

        // Returns the lexically first matching absolute path, if any.
        [[nodiscard]] std::optional<std::filesystem::path> get_file_named(std::string_view name) const;

        // Resolve an exact registered path, not the first matching basename.
        // Relative input is interpreted against the current working directory.
        [[nodiscard]] std::optional<std::filesystem::path> get_file_at(const std::filesystem::path& path) const;

    private:
        void insert_file(const std::filesystem::path& path);
    };
}
