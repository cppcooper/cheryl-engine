#include <cheryl/tools/console/runtime/report.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
#if defined(__unix__) || defined(__APPLE__)
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace Runtime = CE::Tools::Console::Runtime;

namespace {
    struct Files {
        std::filesystem::path root;
        Files() {
            static std::atomic<unsigned> sequence{0};
            root = std::filesystem::path(CONSOLE_RUNTIME_TEST_ROOT) /
                ("console-io-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                 "-" + std::to_string(sequence.fetch_add(1)));
            std::filesystem::create_directory(root);
        }
        ~Files() {
            std::error_code error;
            std::filesystem::remove_all(root, error);
            EXPECT_FALSE(error) << "Cannot remove owned fixture " << root << ": " << error.message();
            const auto remains = std::filesystem::exists(root, error);
            EXPECT_FALSE(error) << "Cannot check owned fixture cleanup " << root << ": " << error.message();
            EXPECT_FALSE(remains) << "Owned fixture remains: " << root;
        }
        void write(const char* name, std::string_view bytes) {
            std::ofstream output(root / name, std::ios::binary);
            output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            if (!output) throw std::runtime_error("Cannot write fixture");
        }
    };
    constexpr std::string_view passed = "{\"schema\":1,\"final\":true,\"disposition\":\"passed\",\"assertions\":1}";
#if defined(__unix__) || defined(__APPLE__)
    struct SearchOnlyDirectory {
        int descriptor;
        mode_t permissions;
        struct stat original{};
        bool restored = false;
        explicit SearchOnlyDirectory(const std::filesystem::path& path)
            : descriptor(::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC)) {
            if (descriptor < 0) {
                const auto error = errno;
                throw std::system_error(error, std::generic_category(), "Cannot open owned permission fixture");
            }
            if (::fstat(descriptor, &original) != 0) {
                const auto error = errno;
                ::close(descriptor);
                throw std::system_error(error, std::generic_category(), "Cannot inspect owned permission fixture");
            }
            permissions = original.st_mode & 0777;
            if (::fchmod(descriptor, S_IXUSR) != 0) {
                const auto error = errno;
                ::close(descriptor);
                throw std::system_error(error, std::generic_category(), "Cannot restrict owned permission fixture");
            }
        }
        SearchOnlyDirectory(const SearchOnlyDirectory&) = delete;
        SearchOnlyDirectory& operator=(const SearchOnlyDirectory&) = delete;
        bool inspect(struct stat& info) const {
            const auto result = ::fstat(descriptor, &info);
            const auto error = errno;
            EXPECT_EQ(result, 0) << "Cannot inspect fixture fd=" << descriptor << " errno=" << error
                << ": " << std::error_code(error, std::generic_category()).message();
            if (result != 0) return false;
            EXPECT_EQ(info.st_dev, original.st_dev) << "Fixture fd=" << descriptor << " changed device";
            EXPECT_EQ(info.st_ino, original.st_ino) << "Fixture fd=" << descriptor << " changed inode";
            return info.st_dev == original.st_dev && info.st_ino == original.st_ino;
        }
        bool restore() {
            if (restored) return true;
            struct stat info{};
            if (!inspect(info)) return false;
            const auto result = ::fchmod(descriptor, permissions);
            const auto error = errno;
            EXPECT_EQ(result, 0) << "Cannot restore owned fixture permissions: fd=" << descriptor
                << " mode=" << permissions << " errno=" << error << ": "
                << std::error_code(error, std::generic_category()).message();
            if (result != 0 || !inspect(info)) return false;
            EXPECT_EQ(info.st_mode & 07777, permissions);
            EXPECT_EQ(info.st_mode & (S_ISUID | S_ISGID), 0);
            restored = (info.st_mode & 07777) == permissions;
            return restored;
        }
        ~SearchOnlyDirectory() {
            static_cast<void>(restore());
            const auto result = ::close(descriptor);
            const auto error = errno;
            EXPECT_EQ(result, 0) << "Cannot close fixture fd=" << descriptor << " errno=" << error;
        }
    };
#endif
}

#if defined(__unix__) || defined(__APPLE__)
TEST(ConsoleRuntimeIo, RestoresSearchOnlyFixture) {
    struct FixtureExit {};
    for (const bool unwind : {false, true}) {
        SCOPED_TRACE(unwind ? "exception unwind" : "normal exit");
        std::filesystem::path root;
        {
            Files files;
            root = files.root;
            const auto ancestor = root / "search";
            std::filesystem::create_directories(ancestor / "child");
            files.write("search/child/input.console", "source");
            struct stat parent{}, original{};
            ASSERT_EQ(::stat(root.c_str(), &parent), 0);
            ASSERT_EQ(::stat(ancestor.c_str(), &original), 0);
            if (parent.st_mode & S_ISGID) EXPECT_NE(original.st_mode & S_ISGID, 0);
            bool unwound = false;
            try {
                SearchOnlyDirectory restricted(ancestor);
                EXPECT_EQ(restricted.permissions, original.st_mode & 0777);
                EXPECT_EQ(restricted.permissions & (S_ISUID | S_ISGID), 0);
                struct stat info{};
                ASSERT_TRUE(restricted.inspect(info));
                ASSERT_EQ(info.st_mode & 07777, S_IXUSR);
                ASSERT_EQ(info.st_uid, ::geteuid());
                ASSERT_EQ(info.st_dev, original.st_dev);
                ASSERT_EQ(info.st_ino, original.st_ino);
                if (unwind) throw FixtureExit{};
            } catch (const FixtureExit&) { unwound = true; }
            EXPECT_EQ(unwound, unwind);
            struct stat info{};
            ASSERT_EQ(::stat(ancestor.c_str(), &info), 0);
            EXPECT_EQ(info.st_dev, original.st_dev);
            EXPECT_EQ(info.st_ino, original.st_ino);
            EXPECT_EQ(info.st_mode & 07777, original.st_mode & 0777);
            EXPECT_EQ(info.st_mode & (S_ISUID | S_ISGID), 0);
        }
        EXPECT_FALSE(std::filesystem::exists(root));
    }
}

TEST(ConsoleRuntimeIo, TraversesSearchOnlyAncestorsAndRelativeDirectories) {
#if (defined(__linux__) && defined(O_PATH)) || defined(O_SEARCH)
    std::filesystem::path root;
    {
        Files files;
        root = files.root;
        const auto ancestor = files.root / "search";
        const auto child = ancestor / "child";
        std::filesystem::create_directories(child);
        files.write("search/child/input.console", "source");
        ASSERT_EQ(Runtime::read_source(child, "input.console"), "source");
        SearchOnlyDirectory restricted(ancestor);
        EXPECT_EQ(restricted.permissions, restricted.original.st_mode & 0777);
        EXPECT_EQ(restricted.permissions & (S_ISUID | S_ISGID), 0);
        struct stat info{};
        ASSERT_EQ(::fstat(restricted.descriptor, &info), 0);
        ASSERT_EQ(info.st_mode & 0777, S_IXUSR);
        ASSERT_EQ(info.st_uid, ::geteuid());
        const auto readable = ::open(ancestor.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        const auto open_error = errno;
        if (readable >= 0) {
            ::close(readable);
            GTEST_SKIP() << "Runtime can read-open an owned mode-0100 directory; privilege or filesystem semantics bypass read denial";
        }
        ASSERT_EQ(open_error, EACCES) << "Fixture must deny directory reading, not fail for an unrelated reason";
        ASSERT_EQ(Runtime::read_source(child, "input.console"), "source");
        ASSERT_EQ(Runtime::read_source(files.root, "search/child/input.console"), "source");
        ASSERT_EQ(Runtime::read_source(ancestor, "child/input.console"), "source");
        {
            Runtime::Report report(child, "run", Runtime::max_output_bytes);
            report.transcript("source\n");
            EXPECT_FALSE(Runtime::completed_report(child, "run", "source", "build"));
            report.finish(passed, "source", "build");
            EXPECT_TRUE(Runtime::completed_report(child, "run", "source", "build"));
            EXPECT_EQ(Runtime::read_source(child, "run/transcript.txt"), "source\n");
            EXPECT_THROW(static_cast<void>(Runtime::read_source(ancestor, "child/../child/input.console")), std::runtime_error);
            EXPECT_THROW(static_cast<void>(Runtime::read_source(ancestor, "child/missing.console")), std::runtime_error);
        }
        ASSERT_TRUE(restricted.restore());
        ASSERT_TRUE(restricted.inspect(info));
        EXPECT_EQ(info.st_mode & 07777, restricted.permissions);
    }
    EXPECT_FALSE(std::filesystem::exists(root));
#else
    GTEST_SKIP() << "Platform exposes neither Linux O_PATH nor POSIX O_SEARCH; readable-directory confinement remains supported";
#endif
}
#endif

TEST(ConsoleRuntimeIo, FinalizesAndChecksIdentity) {
    Files files;
    Runtime::Report report(files.root, "run", Runtime::max_output_bytes);
    report.transcript("actual transcript\n");
    EXPECT_FALSE(Runtime::completed_report(files.root, "run", "source", "build"));
    report.running("{\"final\":false}");
    report.finish(passed, "source", "build");
    EXPECT_TRUE(Runtime::completed_report(files.root, "run", "source", "build"));
    EXPECT_FALSE(Runtime::completed_report(files.root, "run", "other-source", "build"));
    EXPECT_FALSE(Runtime::completed_report(files.root, "run", "source", "other-build"));
    EXPECT_THROW(static_cast<void>(Runtime::Report(files.root, "run", Runtime::max_output_bytes)), std::runtime_error);
    EXPECT_TRUE(report.invalidate());
    EXPECT_FALSE(Runtime::completed_report(files.root, "run", "source", "build"));
}

TEST(ConsoleRuntimeIo, RejectsUnsafeInputs) {
    Files files;
    files.write("valid.console", "valid UTF-8\n");
    ASSERT_EQ(Runtime::read_source(files.root, "valid.console"), "valid UTF-8\n");
    EXPECT_EQ(Runtime::read_source(files.root, "valid.console"), "valid UTF-8\n");
    std::filesystem::create_directory(files.root / "sub");
    for (const auto* name : {"../valid.console", "/valid.console", "sub/../valid.console", "missing.console",
                             "valid.console:stream", "sub\\valid.console"}) {
        SCOPED_TRACE(name);
        EXPECT_THROW(static_cast<void>(Runtime::read_source(files.root, name)), std::runtime_error);
    }
    files.write("invalid.console", std::string(1, char(0xff)));
    EXPECT_THROW(static_cast<void>(Runtime::read_source(files.root, "invalid.console")), std::runtime_error);
    files.write("large.console", std::string(CE::Tools::Console::Scenario::max_file_bytes + 1, 'x'));
    EXPECT_THROW(static_cast<void>(Runtime::read_source(files.root, "large.console")), std::runtime_error);
    std::filesystem::create_directory(files.root / "directory");
    EXPECT_THROW(static_cast<void>(Runtime::read_source(files.root, "directory")), std::runtime_error);
}

TEST(ConsoleRuntimeIo, RejectsLinkedInputs) {
    Files files;
    files.write("source.console", "source");
    ASSERT_EQ(Runtime::read_source(files.root, "source.console"), "source");
    std::error_code error;
    std::filesystem::create_hard_link(files.root / "source.console", files.root / "hard.console", error);
    if (error) GTEST_SKIP() << "Fixture volume does not support hard links: " << error.message();
    EXPECT_THROW(static_cast<void>(Runtime::read_source(files.root, "hard.console")), std::runtime_error);
    EXPECT_THROW(static_cast<void>(Runtime::read_source(files.root, "source.console")), std::runtime_error);
}

TEST(ConsoleRuntimeIo, RejectsSymlinkTraversal) {
    Files files;
    std::filesystem::create_directory(files.root / "source");
    files.write("source/input.console", "source");
    ASSERT_EQ(Runtime::read_source(files.root / "source", "input.console"), "source");
    ASSERT_EQ(Runtime::read_source(files.root, "source/input.console"), "source");
    std::error_code error;
    std::filesystem::create_directory_symlink(files.root / "source", files.root / "linked", error);
    if (error) GTEST_SKIP() << "Symlink fixture privilege unavailable: " << error.message();
    EXPECT_THROW(static_cast<void>(Runtime::read_source(files.root, "linked/input.console")), std::runtime_error);
    EXPECT_THROW(static_cast<void>(Runtime::read_source(files.root / "linked", "input.console")), std::runtime_error);
}

TEST(ConsoleRuntimeIo, OutputFailureCannotComplete) {
    Files files;
    Runtime::Report report(files.root, "full", 1024);
    EXPECT_THROW(report.transcript(std::string(1025, 'x')), std::runtime_error);
    EXPECT_FALSE(Runtime::completed_report(files.root, "full", "source", "build"));
    EXPECT_TRUE(report.invalidate());
    EXPECT_THROW(static_cast<void>(Runtime::Report(files.root / "missing", "run", 1024)), std::runtime_error);
}

#if defined(_WIN32)
TEST(ConsoleRuntimeIo, RejectsWindowsAliasesAndOpenWriters) {
    Files files;
    files.write("valid.console", "source");
    ASSERT_EQ(Runtime::read_source(files.root, "valid.console"), "source");
    Runtime::Report report(files.root, "valid", 1024);
    report.finish(passed, "source", "build");
    ASSERT_TRUE(Runtime::completed_report(files.root, "valid", "source", "build"));
    for (const auto* name : {"NUL", "con.txt", "COM1.console", "lPt9", "trailing."}) {
        EXPECT_THROW(static_cast<void>(Runtime::read_source(files.root, name)), std::runtime_error);
        EXPECT_THROW(static_cast<void>(Runtime::Report(files.root, name, 1024)), std::runtime_error);
    }
    files.write("input.console", "source");
    std::ofstream writer(files.root / "input.console", std::ios::app);
    ASSERT_TRUE(writer);
    EXPECT_THROW(static_cast<void>(Runtime::read_source(files.root, "input.console")), std::runtime_error);
}
#endif
