#include "../src/settings.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <new>
#include <utility>
#include <vector>

namespace {
    using namespace Tutorial;
    using Key = CE::Input::KeyboardKey;
    namespace fs = std::filesystem;
    using Json = nlohmann::json;

    const std::string document = R"({"version":1,"camera_pan_speed":200,"show_health":false,"bindings":{"add_score":"b"}})";
    const GameSettings changed{200, false, Key::B};

    class Files {
        fs::path root_;

    public:
        Files() {
            const auto seed = std::chrono::steady_clock::now().time_since_epoch().count();
            for (unsigned int attempt = 0;; ++attempt) {
                root_ = fs::absolute("tutorial-settings-" + std::to_string(seed) + "-" + std::to_string(attempt));
                if (fs::create_directory(root_))
                    break;
            }
        }
        ~Files() {
            std::error_code error;
            static_cast<void>(fs::remove_all(root_, error));
        }
        fs::path root() const { return root_; }
        fs::path path() const { return root_ / "settings.json"; }
        fs::path pending() const { return root_ / "settings.json.pending"; }
        void write(std::string_view bytes) const {
            std::ofstream output(path(), std::ios::binary);
            output.exceptions(std::ios::badbit | std::ios::failbit);
            output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            output.close();
        }
        static std::string read(const fs::path& path) {
            std::ifstream input(path, std::ios::binary);
            if (!input)
                throw std::runtime_error("Fixture cannot read " + path.string());
            return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        }
    };

    enum class Fault { None, Open, Write, Flush, Close, Rename, Allocation, Programming };

    class Operations final : public Detail::SettingsFileOperations {
    public:
        Fault fault = Fault::None;
        bool cleanup_fails = false;
        bool cleaned_owned_path = false;
        unsigned int opens = 0, writes = 0, flushes = 0, closes = 0, renames = 0, cleanups = 0;
        fs::path opened;

        static void fail() { throw std::system_error(std::make_error_code(std::errc::no_space_on_device), "injected I/O failure"); }
        std::FILE* open_exclusive(const fs::path& path) override {
            ++opens;
            opened = path;
            if (fault == Fault::Open)
                fail();
            return SettingsFileOperations::open_exclusive(path);
        }
        void write(std::FILE* file, std::string_view bytes) override {
            ++writes;
            if (fault == Fault::Allocation)
                throw std::bad_alloc();
            if (fault == Fault::Programming)
                throw std::logic_error("injected programming failure");
            if (fault == Fault::Write) {
                SettingsFileOperations::write(file, bytes.substr(0, bytes.size() / 2));
                fail();
            }
            SettingsFileOperations::write(file, bytes);
        }
        void flush(std::FILE* file) override {
            ++flushes;
            if (fault == Fault::Flush)
                fail();
            SettingsFileOperations::flush(file);
        }
        void close(std::FILE* file) override {
            ++closes;
            SettingsFileOperations::close(file);
            if (fault == Fault::Close)
                fail();
        }
        void replace(const fs::path& temporary, const fs::path& target) override {
            ++renames;
            if (fault == Fault::Rename)
                fail();
            SettingsFileOperations::replace(temporary, target);
        }
        void remove(const fs::path& path, std::error_code& error) noexcept override {
            ++cleanups;
            cleaned_owned_path = path == opened;
            if (cleanup_fails)
                error = std::make_error_code(std::errc::permission_denied);
            else
                SettingsFileOperations::remove(path, error);
        }
    };

    void recovered(const Files& files, std::string_view bytes, std::string_view diagnostic = "$") {
        files.write(bytes);
        const auto result = SettingsFile(files.path()).load_or_defaults();
        EXPECT_EQ(result.status, SettingsLoadStatus::Recovered);
        EXPECT_EQ(result.value, GameSettings{});
        EXPECT_NE(result.issue.find(files.path().string()), std::string::npos);
        EXPECT_NE(result.issue.find(diagnostic), std::string::npos) << result.issue;
        EXPECT_EQ(Files::read(files.path()), bytes);
        EXPECT_FALSE(fs::exists(files.pending()));
    }
}

TEST(tutorial_settings, defaults) {
    const GameSettings value;
    EXPECT_FLOAT_EQ(value.camera_pan_speed, 100);
    EXPECT_TRUE(value.show_health);
    EXPECT_EQ(value.add_score, Key::Space);
    EXPECT_NO_THROW(validate_settings(value));
    for (float speed : {25.0f, 400.0f})
        EXPECT_NO_THROW(validate_settings({speed, false, Key::Digit0}));
}

TEST(tutorial_settings, validation) {
    for (float speed :
         {std::nextafter(25.0f, 0.0f), std::nextafter(400.0f, 500.0f), -1.0f, std::numeric_limits<float>::infinity(),
          -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        SCOPED_TRACE(speed);
        const GameSettings value{speed, false, Key::B};
        EXPECT_THROW(validate_settings(value), std::invalid_argument);
    }
    for (auto key :
         {Key::A, Key::D, Key::J, Key::L, Key::Unknown, Key::Escape, Key::Enter, Key::Tab, Key::Left, Key::F2, Key::F3, Key::LeftShift,
          Key::Keypad0, static_cast<Key>(65535)}) {
        const GameSettings value{200, false, key};
        EXPECT_THROW(validate_settings(value), std::invalid_argument);
        EXPECT_EQ(value.add_score, key);
    }
}

TEST(tutorial_settings, missing) {
    Files files;
    const auto result = SettingsFile(files.path()).load_or_defaults();
    EXPECT_EQ(result.status, SettingsLoadStatus::Missing);
    EXPECT_EQ(result.value, GameSettings{});
    EXPECT_TRUE(result.issue.empty());
    EXPECT_TRUE(fs::is_empty(files.root()));
}

TEST(tutorial_settings, round_trip) {
    Files files;
    const SettingsFile file(files.path());
    file.save(changed);
    const auto first_bytes = Files::read(files.path());
    const auto first = file.load_or_defaults();
    EXPECT_EQ(first.status, SettingsLoadStatus::Loaded);
    EXPECT_EQ(first.value, changed);
    EXPECT_TRUE(first.issue.empty());
    file.save(GameSettings{});
    EXPECT_NE(Files::read(files.path()), first_bytes);
    EXPECT_EQ(SettingsFile(files.path()).load_or_defaults().value, GameSettings{});
    EXPECT_FALSE(fs::exists(files.pending()));
    EXPECT_EQ(std::distance(fs::directory_iterator(files.root()), fs::directory_iterator()), 1);
}

TEST(tutorial_settings, keys) {
    Files files;
    const SettingsFile file(files.path());
    for (int code = static_cast<int>(Key::Unknown); code <= static_cast<int>(Key::Menu); ++code) {
        const auto key = static_cast<Key>(code);
        const bool allowed = key == Key::Space ||
                             (key >= Key::A && key <= Key::Z && key != Key::A && key != Key::D && key != Key::J && key != Key::L) ||
                             (key >= Key::Digit0 && key <= Key::Digit9);
        const GameSettings value{25, true, key};
        SCOPED_TRACE(code);
        if (allowed) {
            file.save(value);
            const auto loaded = file.load_or_defaults();
            ASSERT_EQ(loaded.status, SettingsLoadStatus::Loaded) << loaded.issue;
            EXPECT_EQ(loaded.value, value);
        } else
            EXPECT_THROW(validate_settings(value), std::invalid_argument);
    }
}

TEST(tutorial_settings, schema) {
    Files files;
    std::vector<Json> invalid{nullptr, true, 1, "text", Json::array()};
    const Json valid = Json::parse(document);
    for (const auto* field : {"version", "camera_pan_speed", "show_health", "bindings"}) {
        auto missing = valid;
        missing.erase(field);
        invalid.push_back(std::move(missing));
        for (const auto& wrong : std::array<Json, 5>{nullptr, "text", Json::array(), Json::object(), true}) {
            auto value = valid;
            value[field] = wrong;
            if (field == std::string_view("show_health") && wrong.is_boolean())
                continue;
            if (field == std::string_view("bindings") && wrong.is_object())
                continue;
            invalid.push_back(std::move(value));
        }
    }
    for (const Json& version : std::array<Json, 5>{0, 2, -1, 1.0, "1"}) {
        auto value = valid;
        value["version"] = version;
        invalid.push_back(std::move(value));
    }
    for (const auto& fields :
         {Json::object(), Json{{"add_score", 1}}, Json{{"add_score", true}}, Json{{"add_score", nullptr}},
          Json{{"add_score", Json::array()}}, Json{{"add_score", Json::object()}}, Json{{"add_score", "b"}, {"unknown", 1}}}) {
        auto value = valid;
        value["bindings"] = fields;
        invalid.push_back(std::move(value));
    }
    auto unknown = valid;
    unknown["extra"] = 1;
    invalid.push_back(unknown);
    unknown = valid;
    unknown["show_health"] = 1;
    invalid.push_back(unknown);
    for (const auto& value : invalid) {
        const auto bytes = value.dump();
        SCOPED_TRACE(bytes);
        recovered(files, bytes);
    }
}

TEST(tutorial_settings, tokens) {
    Files files;
    for (const auto* token : {"",     "Space", "B",  "a",       "d",     "j",      "l",  "escape", "enter",  "tab",
                              "left", "f2",    "f3", "shift+b", "key_b", "space ", "00", "-1",     "999999", "\xC3\xA9"}) {
        auto value = Json::parse(document);
        value["bindings"]["add_score"] = token;
        SCOPED_TRACE(token);
        recovered(files, value.dump(), "$.bindings.add_score");
    }
}

TEST(tutorial_settings, numbers) {
    Files files;
    for (const auto* number :
         {"24.999999999", "400.000000001", "0", "-1", "1e1000", "-1e1000", "NaN", "Infinity",
          "184467440737095516160000000000000000000000000000000"}) {
        auto bytes = document;
        bytes.replace(bytes.find("200"), 3, number);
        SCOPED_TRACE(number);
        recovered(files, bytes, "$.camera_pan_speed");
    }
}

TEST(tutorial_settings, duplicates) {
    Files files;
    for (const auto* duplicate :
         {R"("version":1,)", R"("camera_pan_speed":75,)", R"("show_health":true,)", R"("bindings":{"add_score":"space"},)",
          R"("\u0073how_health":true,)"}) {
        const auto bytes = "{" + std::string(duplicate) + document.substr(1);
        SCOPED_TRACE(bytes);
        recovered(files, bytes, "duplicate");
    }
    recovered(
        files, R"({"version":1,"camera_pan_speed":200,"show_health":false,"bindings":{"add_score":"b","\u0061dd_score":"space"}})",
        "duplicate"
    );
}

TEST(tutorial_settings, malformed) {
    Files files;
    for (const auto& bytes :
         {std::string{}, document.substr(0, document.size() - 1), document + "x", document + "{}", document + document, document + " false",
          std::string("/*comment*/") + document, std::string("\xFF") + document}) {
        SCOPED_TRACE(bytes);
        recovered(files, bytes);
    }
    files.write(document + "\n\t \r");
    EXPECT_EQ(SettingsFile(files.path()).load_or_defaults().value, changed);
}

TEST(tutorial_settings, nul) {
    Files files;
    for (const auto& suffix : {std::string{}, std::string("junk"), document})
        recovered(files, document + std::string(1, '\0') + suffix, "raw NUL");
    auto inside = document;
    inside.insert(inside.find('b'), 1, '\0');
    recovered(files, inside, "raw NUL");
}

TEST(tutorial_settings, limits) {
    Files files;
    const std::string at_limit = document + std::string(maximum_settings_bytes - document.size(), ' ');
    files.write(at_limit);
    const auto loaded = SettingsFile(files.path()).load_or_defaults();
    ASSERT_EQ(loaded.status, SettingsLoadStatus::Loaded) << loaded.issue;
    EXPECT_EQ(loaded.value, changed);
    recovered(files, at_limit + ' ', "64 KiB");
    recovered(files, document + std::string(1024 * 1024, ' '), "64 KiB");
}

TEST(tutorial_settings, depth) {
    Files files;
    recovered(files, R"({"bindings":{"add_score":)" + std::string(20000, '[') + "0" + std::string(20000, ']') + "}}", "arrays");
    std::string nested = R"({"bindings":{"add_score":)";
    for (int i = 0; i < 5000; ++i)
        nested += R"({"x":)";
    nested += "0" + std::string(5000, '}') + "}}";
    recovered(files, nested, "unexpected object");
}

TEST(tutorial_settings, read_errors) {
    Files files;
    fs::create_directory(files.path());
    const auto directory = SettingsFile(files.path()).load_or_defaults();
    EXPECT_EQ(directory.status, SettingsLoadStatus::Recovered);
    EXPECT_EQ(directory.value, GameSettings{});
    EXPECT_NE(directory.issue.find("read"), std::string::npos);
    EXPECT_TRUE(fs::is_directory(files.path()));
    static_cast<void>(fs::remove(files.path()));
    files.write("parent is a file");
    const auto not_directory = SettingsFile(files.path() / "child.json").load_or_defaults();
    EXPECT_EQ(not_directory.status, SettingsLoadStatus::Recovered);
    EXPECT_EQ(not_directory.value, GameSettings{});
    EXPECT_NE(not_directory.issue.find("inspect"), std::string::npos);
    EXPECT_EQ(Files::read(files.path()), "parent is a file");
#ifndef _WIN32
    fs::create_symlink(files.root() / "absent.json", files.root() / "dangling.json");
    const auto dangling = SettingsFile(files.root() / "dangling.json").load_or_defaults();
    EXPECT_EQ(dangling.status, SettingsLoadStatus::Recovered);
    EXPECT_NE(dangling.issue.find("symlinks"), std::string::npos);
#endif
}

#ifndef _WIN32
TEST(tutorial_settings, symlink_leaf) {
    Files files;
    const auto referent = files.root() / "referent.json";
    {
        std::ofstream output(referent, std::ios::binary);
        output.exceptions(std::ios::badbit | std::ios::failbit);
        output << document;
        output.close();
    }
    fs::create_symlink(referent, files.path());
    const SettingsFile file(files.path());
    const auto rejected = file.load_or_defaults();
    EXPECT_EQ(rejected.status, SettingsLoadStatus::Recovered);
    EXPECT_EQ(rejected.value, GameSettings{});
    EXPECT_NE(rejected.issue.find("symlinks"), std::string::npos);
    EXPECT_EQ(fs::read_symlink(files.path()), referent);
    EXPECT_EQ(Files::read(referent), document);
    file.save(GameSettings{});
    EXPECT_FALSE(fs::is_symlink(files.path()));
    EXPECT_TRUE(fs::is_regular_file(files.path()));
    const auto loaded = file.load_or_defaults();
    EXPECT_EQ(loaded.status, SettingsLoadStatus::Loaded);
    EXPECT_EQ(loaded.value, GameSettings{});
    EXPECT_EQ(Files::read(referent), document);
    EXPECT_FALSE(fs::exists(files.pending()));
}
#endif

TEST(tutorial_settings, invalid_save) {
    Files files;
    files.write(document);
    Operations operations;
    const SettingsFile file(files.path(), operations);
    for (const auto value :
         {GameSettings{24, false, Key::B}, GameSettings{100, false, Key::A},
          GameSettings{std::numeric_limits<float>::quiet_NaN(), false, Key::B}}) {
        EXPECT_THROW(file.save(value), std::invalid_argument);
        EXPECT_EQ(Files::read(files.path()), document);
        EXPECT_FALSE(fs::exists(files.pending()));
    }
    EXPECT_EQ(operations.opens, 0u);
    EXPECT_EQ(operations.cleanups, 0u);
}

TEST(tutorial_settings, collision) {
    Files files;
    files.write(document);
    const std::string sentinel = "belongs to another writer";
    {
        std::ofstream output(files.pending(), std::ios::binary);
        output << sentinel;
    }
    Operations operations;
    const SettingsFile file(files.path(), operations);
    try {
        file.save(GameSettings{});
        FAIL() << "Existing temporary file was overwritten";
    } catch (const SettingsFileError& error) {
        EXPECT_NE(std::string(error.what()).find("open exclusive temporary"), std::string::npos);
        EXPECT_NE(std::string(error.what()).find(files.pending().string()), std::string::npos);
    }
    EXPECT_EQ(Files::read(files.pending()), sentinel);
    EXPECT_EQ(Files::read(files.path()), document);
    EXPECT_EQ(operations.opens, 1u);
    EXPECT_EQ(operations.cleanups, 0u);
}

TEST(tutorial_settings, save_failures) {
    Files files;
    for (const auto& [fault, operation] : std::array{
             std::pair{Fault::Open, "open exclusive temporary"}, std::pair{Fault::Write, "write"}, std::pair{Fault::Flush, "flush"},
             std::pair{Fault::Close, "close"}, std::pair{Fault::Rename, "rename"}
         }) {
        SCOPED_TRACE(operation);
        files.write(document);
        Operations operations;
        operations.fault = fault;
        const SettingsFile file(files.path(), operations);
        try {
            file.save(GameSettings{});
            FAIL() << "Injected failure was ignored";
        } catch (const SettingsFileError& error) {
            EXPECT_NE(std::string(error.what()).find(operation), std::string::npos);
            EXPECT_NE(std::string(error.what()).find(files.path().string()), std::string::npos);
        }
        EXPECT_EQ(Files::read(files.path()), document);
        EXPECT_EQ(file.load_or_defaults().value, changed);
        EXPECT_FALSE(fs::exists(files.pending()));
        EXPECT_EQ(operations.cleanups, fault == Fault::Open ? 0u : 1u);
        EXPECT_EQ(operations.closes, fault == Fault::Open ? 0u : 1u);
        if (fault != Fault::Open) {
            EXPECT_TRUE(operations.cleaned_owned_path);
        }
    }
}

TEST(tutorial_settings, cleanup_failure) {
    Files files;
    files.write(document);
    Operations operations;
    operations.fault = Fault::Write;
    operations.cleanup_fails = true;
    try {
        SettingsFile(files.path(), operations).save(GameSettings{});
        FAIL() << "Injected failure was ignored";
    } catch (const SettingsFileError& error) {
        const std::string issue = error.what();
        EXPECT_NE(issue.find("write"), std::string::npos);
        EXPECT_NE(issue.find("cleanup"), std::string::npos);
        EXPECT_NE(issue.find(files.pending().string()), std::string::npos);
    }
    EXPECT_EQ(Files::read(files.path()), document);
    EXPECT_TRUE(fs::exists(files.pending()));
    EXPECT_TRUE(operations.cleaned_owned_path);
}

TEST(tutorial_settings, commit_point) {
    Files files;
    files.write(document);
    Operations operations;
    operations.cleanup_fails = true;
    EXPECT_NO_THROW(SettingsFile(files.path(), operations).save(GameSettings{}));
    EXPECT_EQ(operations.renames, 1u);
    EXPECT_EQ(operations.cleanups, 0u);
    EXPECT_EQ(operations.closes, 1u);
    EXPECT_EQ(SettingsFile(files.path()).load_or_defaults().value, GameSettings{});
    EXPECT_FALSE(fs::exists(files.pending()));
}

TEST(tutorial_settings, exceptional_failures) {
    Files files;
    files.write(document);
    Operations operations;
    const SettingsFile file(files.path(), operations);
    operations.fault = Fault::Allocation;
    EXPECT_THROW(file.save(GameSettings{}), std::bad_alloc);
    EXPECT_EQ(Files::read(files.path()), document);
    EXPECT_FALSE(fs::exists(files.pending()));
    operations.fault = Fault::Programming;
    EXPECT_THROW(file.save(GameSettings{}), std::logic_error);
    EXPECT_EQ(Files::read(files.path()), document);
    EXPECT_FALSE(fs::exists(files.pending()));
}

TEST(tutorial_settings, relative_path) {
    Files files;
    const auto before = fs::current_path();
    struct Restore {
        fs::path path;
        ~Restore() {
            std::error_code error;
            fs::current_path(path, error);
        }
    } restore{before};
    const auto first = files.root() / "first";
    const auto second = files.root() / "second";
    fs::create_directory(first);
    fs::create_directory(second);
    fs::current_path(first);
    const SettingsFile file("settings.json");
    EXPECT_EQ(file.path(), first / "settings.json");
    fs::current_path(second);
    file.save(changed);
    EXPECT_FALSE(fs::exists(second / "settings.json"));
    EXPECT_EQ(file.load_or_defaults().value, changed);
    EXPECT_TRUE(fs::exists(first / "settings.json"));
    EXPECT_THROW(static_cast<void>(SettingsFile(fs::path{})), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(SettingsFile(fs::path(std::string("settings\0.json", 14)))), std::invalid_argument);
}
