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
