#pragma once

#include <cheryl/core/controls/button-codes.h>

#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace Tutorial {
    inline constexpr float minimum_pan_speed = 25;
    inline constexpr float maximum_pan_speed = 400;
    inline constexpr std::size_t maximum_settings_bytes = 64 * 1024;

    struct GameSettings {
        float camera_pan_speed = 100;
        bool show_health = true;
        CE::Input::KeyboardKey add_score = CE::Input::KeyboardKey::Space;

        bool operator==(const GameSettings&) const = default;
    };

    // Throws invalid_argument with a field name; does not modify the supplied value.
    void validate_settings(const GameSettings& settings);

    enum class SettingsLoadStatus { Missing, Loaded, Recovered };

    struct SettingsLoadResult {
        GameSettings value;
        SettingsLoadStatus status = SettingsLoadStatus::Missing;
        std::string issue;
    };

    class SettingsFileError : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    namespace Detail {
        // App-private failure seam. Errors use system_error; other exceptions propagate.
        // A failed open acquires nothing. close consumes the stream even on failure.
        // replace either fails without replacement or returns after its commit point.
        class SettingsFileOperations {
        public:
            virtual ~SettingsFileOperations() = default;
            virtual std::FILE* open_exclusive(const std::filesystem::path& path);
            virtual void write(std::FILE* file, std::string_view bytes);
            virtual void flush(std::FILE* file);
            virtual void close(std::FILE* file);
            virtual void replace(const std::filesystem::path& temporary, const std::filesystem::path& target);
            virtual void remove(const std::filesystem::path& path, std::error_code& error) noexcept;
        };

        SettingsFileOperations& settings_file_operations();
    }

    class SettingsFile final {
        std::filesystem::path path_;
        std::filesystem::path temporary_;
        Detail::SettingsFileOperations* operations_;

    public:
        // Resolve once against the current directory. The injected operations must outlive this object.
        explicit SettingsFile(std::filesystem::path path, Detail::SettingsFileOperations& operations = Detail::settings_file_operations());

        [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }
        // Missing/invalid/unreadable files never cause a write. Allocation/programming failures propagate.
        [[nodiscard]] SettingsLoadResult load_or_defaults() const;
        // Validate/serialize before exclusive .pending creation. Rename commits; no fallible work follows.
        // Invalid values throw invalid_argument; ordinary I/O failures throw SettingsFileError.
        // Serialize calls for one path; there is no multi-process or power-loss durability guarantee.
        void save(const GameSettings& settings) const;
    };
}
