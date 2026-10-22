#include "settings.h"

#include <nlohmann/json.hpp>

#include <array>
#include <cerrno>
#include <cmath>
#include <exception>
#include <fstream>
#include <optional>
#include <utility>

namespace Tutorial {
    namespace {
        using Json = nlohmann::json;
        using Key = CE::Input::KeyboardKey;
        namespace fs = std::filesystem;

        struct NamedKey {
            Key key;
            std::string_view token;
        };

        constexpr std::array permitted_keys{NamedKey{Key::Space, "space"}, NamedKey{Key::B, "b"},      NamedKey{Key::C, "c"},
                                            NamedKey{Key::E, "e"},         NamedKey{Key::F, "f"},      NamedKey{Key::G, "g"},
                                            NamedKey{Key::H, "h"},         NamedKey{Key::I, "i"},      NamedKey{Key::K, "k"},
                                            NamedKey{Key::M, "m"},         NamedKey{Key::N, "n"},      NamedKey{Key::O, "o"},
                                            NamedKey{Key::P, "p"},         NamedKey{Key::Q, "q"},      NamedKey{Key::R, "r"},
                                            NamedKey{Key::S, "s"},         NamedKey{Key::T, "t"},      NamedKey{Key::U, "u"},
                                            NamedKey{Key::V, "v"},         NamedKey{Key::W, "w"},      NamedKey{Key::X, "x"},
                                            NamedKey{Key::Y, "y"},         NamedKey{Key::Z, "z"},      NamedKey{Key::Digit0, "0"},
                                            NamedKey{Key::Digit1, "1"},    NamedKey{Key::Digit2, "2"}, NamedKey{Key::Digit3, "3"},
                                            NamedKey{Key::Digit4, "4"},    NamedKey{Key::Digit5, "5"}, NamedKey{Key::Digit6, "6"},
                                            NamedKey{Key::Digit7, "7"},    NamedKey{Key::Digit8, "8"}, NamedKey{Key::Digit9, "9"}};

        std::string_view key_token(Key key) {
            for (const auto& entry : permitted_keys)
                if (entry.key == key)
                    return entry.token;
            return {};
        }

        std::optional<Key> key_from_token(std::string_view token) {
            for (const auto& entry : permitted_keys)
                if (entry.token == token)
                    return entry.key;
            return std::nullopt;
        }

        [[noreturn]] void file_error(const fs::path& path, std::string_view operation, std::string_view reason) {
            throw SettingsFileError("Settings '" + path.string() + "' " + std::string(operation) + ": " + std::string(reason));
        }

        [[noreturn]] void io_error() {
            throw std::system_error(errno ? errno : EIO, std::generic_category());
        }

        template <typename Work> decltype(auto) file_operation(const fs::path& path, std::string_view name, Work&& work) {
            try {
                return work();
            } catch (const std::system_error& error) {
                file_error(path, name, error.what());
            }
        }

        // SAX admits only a root object, one bindings object, and the four scalar fields.
        // No nested DOM is built, and duplicate keys are compared after JSON decoding.
        class SettingsReader final : public nlohmann::json_sax<Json> {
            enum class Field { None, Version, Speed, Health, Bindings, Score };
            Field field_ = Field::None;
            unsigned int depth_ = 0;
            unsigned int fields_ = 0;
            bool score_seen_ = false;
            bool root_seen_ = false;
            GameSettings value_;
            std::string issue_;

            std::string_view location() const {
                switch (field_) {
                    case Field::Version:
                        return "$.version";
                    case Field::Speed:
                        return "$.camera_pan_speed";
                    case Field::Health:
                        return "$.show_health";
                    case Field::Bindings:
                        return "$.bindings";
                    case Field::Score:
                        return "$.bindings.add_score";
                    default:
                        return depth_ == 2 ? "$.bindings" : "$";
                }
            }

            bool fail(std::string_view reason) {
                issue_ = std::string(location()) + ": " + std::string(reason);
                return false;
            }

            bool integer_version(bool is_one) {
                if (field_ != Field::Version || !is_one)
                    return fail("expected integer version 1");
                field_ = Field::None;
                return true;
            }

            bool speed(double value) {
                if (field_ != Field::Speed || !std::isfinite(value) || value < minimum_pan_speed || value > maximum_pan_speed)
                    return fail("expected finite camera pan speed in [25, 400]");
                value_.camera_pan_speed = static_cast<float>(value);
                field_ = Field::None;
                return true;
            }

        public:
            [[nodiscard]] const GameSettings& value() const { return value_; }
            [[nodiscard]] const std::string& issue() const { return issue_; }

            bool null() override { return fail("null is not a settings value"); }
            bool boolean(bool value) override {
                if (field_ != Field::Health)
                    return fail("unexpected boolean");
                value_.show_health = value;
                field_ = Field::None;
                return true;
            }
            bool number_integer(number_integer_t value) override {
                return field_ == Field::Speed ? speed(static_cast<double>(value)) : integer_version(value == 1);
            }
            bool number_unsigned(number_unsigned_t value) override {
                return field_ == Field::Speed ? speed(static_cast<double>(value)) : integer_version(value == 1);
            }
            bool number_float(number_float_t value, const string_t&) override {
                if (field_ == Field::Version)
                    return fail("expected integer version 1");
                return speed(value);
            }
            bool string(string_t& token) override {
                if (field_ != Field::Score)
                    return fail("unexpected string");
                const auto key = key_from_token(token);
                if (!key)
                    return fail("unsupported or reserved physical key token");
                value_.add_score = *key;
                field_ = Field::None;
                return true;
            }
            bool binary(binary_t&) override { return fail("binary values are unsupported"); }
            bool start_array(std::size_t) override { return fail("arrays are unsupported"); }
            bool end_array() override { return fail("arrays are unsupported"); }

            bool start_object(std::size_t) override {
                if (!root_seen_ && depth_ == 0) {
                    root_seen_ = true;
                    depth_ = 1;
                    return true;
                }
                if (depth_ == 1 && field_ == Field::Bindings) {
                    depth_ = 2;
                    field_ = Field::None;
                    return true;
                }
                return fail("unexpected object; settings schema permits only root and bindings objects");
            }

            bool key(string_t& key) override {
                if (depth_ == 2) {
                    if (key != "add_score")
                        return fail("unknown property '" + key + "'");
                    field_ = Field::Score;
                    if (std::exchange(score_seen_, true))
                        return fail("duplicate property");
                    return true;
                }
                unsigned int bit = 0;
                if (key == "version") {
                    field_ = Field::Version;
                    bit = 1;
                } else if (key == "camera_pan_speed") {
                    field_ = Field::Speed;
                    bit = 2;
                } else if (key == "show_health") {
                    field_ = Field::Health;
                    bit = 4;
                } else if (key == "bindings") {
                    field_ = Field::Bindings;
                    bit = 8;
                } else
                    return fail("unknown property '" + key + "'");
                if ((fields_ & bit) != 0)
                    return fail("duplicate property");
                fields_ |= bit;
                return true;
            }

            bool end_object() override {
                if (depth_ == 2) {
                    if (!score_seen_)
                        return fail("missing add_score property");
                    depth_ = 1;
                } else {
                    if (fields_ != 15)
                        return fail("missing required version, camera_pan_speed, show_health, or bindings property");
                    depth_ = 0;
                }
                field_ = Field::None;
                return true;
            }

            bool parse_error(std::size_t, const std::string&, const Json::exception& error) override { return fail(error.what()); }
        };

        GameSettings parse_settings(std::string_view bytes, const fs::path& path) {
            // The pinned parser treats raw NUL as EOF, even in strict mode.
            if (bytes.find('\0') != std::string_view::npos)
                file_error(path, "parse $", "raw NUL byte is forbidden");
            SettingsReader reader;
            if (!Json::sax_parse(bytes.begin(), bytes.end(), &reader))
                file_error(path, "parse", reader.issue());
            const auto value = reader.value();
            try {
                validate_settings(value);
            } catch (const std::invalid_argument& error) {
                file_error(path, "validate", error.what());
            }
            return value;
        }

        std::string serialize_settings(const GameSettings& value) {
            return Json{
                       {"version", 1},
                       {"camera_pan_speed", value.camera_pan_speed},
                       {"show_health", value.show_health},
                       {"bindings", {{"add_score", key_token(value.add_score)}}}
                   }.dump(2) +
                   '\n';
        }
    }

    void validate_settings(const GameSettings& settings) {
        if (!std::isfinite(settings.camera_pan_speed) || settings.camera_pan_speed < minimum_pan_speed ||
            settings.camera_pan_speed > maximum_pan_speed)
            throw std::invalid_argument("$.camera_pan_speed: expected finite world units/second in [25, 400]");
        if (key_token(settings.add_score).empty())
            throw std::invalid_argument("$.bindings.add_score: unsupported or reserved physical key");
    }

    namespace Detail {
        std::FILE* SettingsFileOperations::open_exclusive(const fs::path& path) {
            errno = 0;
#ifdef _WIN32
            auto* file = _wfopen(path.c_str(), L"wbx");
#else
            auto* file = std::fopen(path.c_str(), "wbx");
#endif
            if (!file)
                io_error();
            return file;
        }

        void SettingsFileOperations::write(std::FILE* file, std::string_view bytes) {
            errno = 0;
            if (std::fwrite(bytes.data(), 1, bytes.size(), file) != bytes.size())
                io_error();
        }

        void SettingsFileOperations::flush(std::FILE* file) {
            errno = 0;
            if (std::fflush(file) != 0)
                io_error();
        }

        void SettingsFileOperations::close(std::FILE* file) {
            errno = 0;
            if (std::fclose(file) != 0)
                io_error();
        }

        void SettingsFileOperations::replace(const fs::path& temporary, const fs::path& target) {
            fs::rename(temporary, target);
        }
        void SettingsFileOperations::remove(const fs::path& path, std::error_code& error) noexcept {
            static_cast<void>(fs::remove(path, error));
        }
        SettingsFileOperations& settings_file_operations() {
            static SettingsFileOperations operations;
            return operations;
        }
    }

    SettingsFile::SettingsFile(fs::path path, Detail::SettingsFileOperations& operations)
    : operations_(&operations) {
        if (path.empty() || path.native().find(fs::path::value_type{}) != fs::path::string_type::npos)
            throw std::invalid_argument("Settings file path must not be empty or contain NUL");
        path_ = fs::absolute(std::move(path)).lexically_normal();
        temporary_ = path_;
        temporary_ += ".pending";
    }

    SettingsLoadResult SettingsFile::load_or_defaults() const {
        try {
            std::error_code error;
            const auto status = fs::symlink_status(path_, error);
            if (error == std::errc::no_such_file_or_directory || (!error && status.type() == fs::file_type::not_found))
                return {};
            if (error)
                file_error(path_, "inspect", error.message());
            if (!fs::is_regular_file(status))
                file_error(path_, "read", "expected a regular file (symlinks are unsupported)");
            errno = 0;
            std::ifstream input(path_, std::ios::binary);
            if (!input)
                file_error(path_, "open for read", errno ? std::generic_category().message(errno) : "unable to open file");
            std::array<char, maximum_settings_bytes + 1> bytes;
            input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            const auto size = static_cast<std::size_t>(input.gcount());
            if (input.bad() || (input.fail() && !input.eof()))
                file_error(path_, "read", "unable to read settings bytes");
            if (size > maximum_settings_bytes)
                file_error(path_, "read $", "settings file exceeds 64 KiB");
            return {parse_settings({bytes.data(), size}, path_), SettingsLoadStatus::Loaded, {}};
        } catch (const SettingsFileError& error) {
            return {GameSettings{}, SettingsLoadStatus::Recovered, error.what()};
        }
    }

    void SettingsFile::save(const GameSettings& settings) const {
        validate_settings(settings);
        const auto bytes = serialize_settings(settings);
        auto* output = file_operation(temporary_, "open exclusive temporary", [&] { return operations_->open_exclusive(temporary_); });
        if (!output)
            throw std::logic_error("Settings file operations returned an empty stream");
        try {
            file_operation(temporary_, "write", [&] { operations_->write(output, bytes); });
            file_operation(temporary_, "flush", [&] { operations_->flush(output); });
            auto* closing = std::exchange(output, nullptr);
            file_operation(temporary_, "close", [&] { operations_->close(closing); });
            file_operation(path_, "rename temporary", [&] { operations_->replace(temporary_, path_); });
            // Rename committed. Returning performs no allocation, cleanup, or other fallible operation.
        } catch (...) {
            const auto cause = std::current_exception();
            if (output) {
                try {
                    operations_->close(std::exchange(output, nullptr));
                } catch (...) {
                    // Preserve the original failure; close relinquishes the stream even on failure.
                }
            }
            std::error_code cleanup;
            operations_->remove(temporary_, cleanup);
            if (cleanup) {
                try {
                    std::rethrow_exception(cause);
                } catch (const SettingsFileError& error) {
                    file_error(temporary_, "cleanup after failed save", std::string(error.what()) + "; " + cleanup.message());
                }
            }
            std::rethrow_exception(cause);
        }
    }
}
