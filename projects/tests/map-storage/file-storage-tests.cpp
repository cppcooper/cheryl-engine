#include <cheryl/core/resources/map-storage/file-storage.h>
#include "file-operations.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <utility>

#if defined(__linux__)
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace {
    namespace MF = CE::Maps;
    namespace fs = std::filesystem;

    MF::Snapshot fixture() {
        return {
            .id = "fixture:two-layers", .width = 3, .height = 2,
            .layers = {
                {.id = "ground", .cells = {"fixture:grass", "fixture:stone", "fixture:grass",
                                          "fixture:grass", "fixture:grass", "fixture:stone"}},
                {.id = "overlay", .cells = {std::nullopt, "fixture:wall", std::nullopt,
                                           std::nullopt, std::nullopt, "fixture:wall"}}
            },
            .spawns = {{.name = "entry", .layer = "ground", .x = 1, .y = 1}}
        };
    }

    MF::Snapshot replacement() {
        auto value = fixture();
        value.id = "fixture:replacement";
        value.layers[0].cells[0] = "unbound:new-tile";
        value.layers[1].cells[4] = "unbound:overlay";
        value.spawns[0].x = 2;
        return value;
    }

    template<class Action>
    MF::Error rejected(Action&& action, MF::Code code, const std::string& operation,
                       const fs::path& origin, const std::string& location) {
        try {
            action();
        } catch (const MF::Error& error) {
            EXPECT_EQ(error.code, code);
            EXPECT_EQ(error.operation, operation);
            EXPECT_EQ(error.origin, origin);
            EXPECT_TRUE(error.location.starts_with(location)) << error.location;
            const std::string message = error.what();
            auto displayed = origin.string();
            for (std::size_t at = 0; (at = displayed.find('\0', at)) != std::string::npos; at += 2)
                displayed.replace(at, 1, "\\0");
            EXPECT_NE(message.find(operation), std::string::npos);
            EXPECT_NE(message.find(displayed), std::string::npos);
            EXPECT_NE(message.find(error.location), std::string::npos);
            return error;
        }
        throw std::logic_error("operation unexpectedly accepted");
    }

    enum class Fault { None, Create, PartialWrite, ZeroWrite, Close, Replace, ReplaceInterrupted, Cleanup, NativeRename };

    struct FaultOps final : MF::Detail::FileOps {
        Fault fault = Fault::None;
        bool short_reads = false;
        bool short_writes = false;
        bool interrupt_read = false;
        bool interrupt_write = false;
        bool fail_open = false;
        bool fail_inspect = false;
        std::optional<std::size_t> read_failure_after;
        fs::path collision;
        fs::path reuse_after_close;
        std::array<char, 4096> staging_name {};
        int reused_fd = -1;
        int status_calls = 0;
        int open_calls = 0;
        int inspect_calls = 0;
        int create_calls = 0;
        int read_calls = 0;
        int write_calls = 0;
        int close_calls = 0;
        int replace_calls = 0;
        int native_replace_calls = 0;
        int native_replace_error = 0;
        int remove_calls = 0;
        std::size_t bytes_read = 0;
        std::size_t bytes_written = 0;

        ~FaultOps() override {
            if (reused_fd >= 0)
                static_cast<void>(::close(reused_fd));
        }

        int status(const char* path, struct stat* state, bool follow) noexcept override {
            ++status_calls;
            return FileOps::status(path, state, follow);
        }
        int open_read(const char* path) noexcept override {
            ++open_calls;
            if (fail_open) {
                errno = EACCES;
                return -1;
            }
            return FileOps::open_read(path);
        }
        int inspect(int fd, struct stat* state) noexcept override {
            ++inspect_calls;
            if (fail_inspect) {
                errno = EIO;
                return -1;
            }
            return FileOps::inspect(fd, state);
        }
        int create(char* name) noexcept override {
            ++create_calls;
            if (fault == Fault::Create) {
                errno = EACCES;
                return -1;
            }
            if (!collision.empty()) {
                // Test path has exactly the template's length: no buffer resizing.
                std::memcpy(name, collision.c_str(), collision.native().size() + 1);
                std::snprintf(staging_name.data(), staging_name.size(), "%s", name);
                return ::open(name, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
            }
            const auto fd = FileOps::create(name);
            std::snprintf(staging_name.data(), staging_name.size(), "%s", name);
            return fd;
        }
        ssize_t read(int fd, void* data, std::size_t count) noexcept override {
            ++read_calls;
            if (std::exchange(interrupt_read, false)) {
                errno = EINTR;
                return -1;
            }
            if (read_failure_after && bytes_read >= *read_failure_after) {
                errno = EIO;
                return -1;
            }
            if (short_reads)
                count = std::min(count, std::size_t{3});
            const auto got = FileOps::read(fd, data, count);
            if (got > 0)
                bytes_read += static_cast<std::size_t>(got);
            return got;
        }
        ssize_t write(int fd, const void* data, std::size_t count) noexcept override {
            ++write_calls;
            if (std::exchange(interrupt_write, false)) {
                errno = EINTR;
                return -1;
            }
            if (fault == Fault::PartialWrite) {
                if (bytes_written > 0) {
                    errno = ENOSPC;
                    return -1;
                }
                count = std::min(count, std::size_t{7});
            }
            if (fault == Fault::ZeroWrite)
                return 0;
            if (short_writes)
                count = std::min(count, std::size_t{3});
            const auto wrote = FileOps::write(fd, data, count);
            if (wrote > 0)
                bytes_written += static_cast<std::size_t>(wrote);
            return wrote;
        }
        int close(int fd) noexcept override {
            ++close_calls;
            const auto result = FileOps::close(fd);
            if (result == 0 && fault == Fault::Close) {
                if (!reuse_after_close.empty()) {
                    reused_fd = ::open(reuse_after_close.c_str(), O_RDONLY | O_CLOEXEC);
                    if (reused_fd >= 0 && reused_fd != fd) {
                        const auto duplicate = ::dup2(reused_fd, fd);
                        static_cast<void>(::close(reused_fd));
                        reused_fd = duplicate;
                    }
                }
                errno = EINTR; // Linux ownership must be relinquished, not retried.
                return -1;
            }
            return result;
        }
        int replace(const char* from, const char* to) noexcept override {
            ++replace_calls;
            if (fault == Fault::Replace || fault == Fault::Cleanup || fault == Fault::ReplaceInterrupted) {
                errno = fault == Fault::ReplaceInterrupted ? EINTR : EACCES;
                return -1;
            }
            if (fault == Fault::NativeRename) {
                // Remove only the entry this call owns, then reach the real syscall.
                // ENOENT is deterministic regardless of effective uid/permissions.
                if (::unlink(from) != 0)
                    return -1;
            }
            ++native_replace_calls;
            const auto result = FileOps::replace(from, to);
            native_replace_error = result == 0 ? 0 : errno;
            return result;
        }
        int remove(const char* path) noexcept override {
            ++remove_calls;
            if (fault == Fault::Cleanup) {
                errno = EBUSY;
                return -1;
            }
            return FileOps::remove(path);
        }
    };

    class files : public testing::Test {
    protected:
        fs::path directory;
        fs::path target;
        fs::path unrelated;

        files() {
            auto name = (fs::current_path() / "map-files-XXXXXX").native();
            if (!::mkdtemp(name.data()))
                throw std::system_error(errno, std::generic_category(), "create disposable test directory");
            directory = name;
            target = directory / "fixture.map.json";
            unrelated = directory / ".map-storage-OTHER0";
            put(unrelated, "unrelated staging-like bytes");
        }
        ~files() override {
            std::error_code ignored;
            fs::remove_all(directory, ignored); // Only this fixture's private leaf.
        }
        static void put(const fs::path& path, const std::string& bytes) {
            std::ofstream output(path, std::ios::binary);
            output.exceptions(std::ios::failbit | std::ios::badbit);
            output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            output.close();
        }
        static std::string bytes(const fs::path& path) {
            std::ifstream input(path, std::ios::binary);
            input.exceptions(std::ios::badbit);
            if (!input.is_open())
                throw std::runtime_error("cannot read test fixture bytes");
            return {std::istreambuf_iterator<char>(input), {}};
        }
        std::vector<fs::path> entries() const {
            std::vector<fs::path> result;
            for (const auto& entry : fs::directory_iterator(directory))
                result.push_back(entry.path());
            std::ranges::sort(result);
            return result;
        }
        void intact(const std::string& previous) const {
            EXPECT_EQ(bytes(target), previous);
            EXPECT_EQ(MF::load(target), fixture());
            EXPECT_EQ(bytes(unrelated), "unrelated staging-like bytes");
        }

        void ancestor_parent(bool relative, bool existing) {
            const auto cwd = fs::current_path();
            ASSERT_EQ(directory.parent_path(), cwd);
            const auto base = directory / "base";
            const auto other = directory / "other";
            fs::create_directory(base);
            fs::create_directories(other / "sub");
            fs::create_directory_symlink(other / "sub", base / "link");
            const auto native_target = other / "map.json";
            const auto lexical_target = base / "map.json";
            const auto a = fixture();
            const auto b = replacement();
            auto c = replacement();
            c.id = "fixture:through-link";
            c.layers[0].cells[1] = "fixture:third";
            c.spawns[0].y = 0;
            const auto a_bytes = MF::encode(a) + " \n";
            const auto b_bytes = MF::encode(b) + "\t\n";
            const auto c_bytes = MF::encode(c);
            put(lexical_target, b_bytes); // Valid byte sentinel at the other candidate.
            ASSERT_EQ(MF::load(lexical_target), b);
            if (existing) {
                put(native_target, a_bytes);
                ASSERT_EQ(MF::load(native_target), a);
            } else {
                ASSERT_FALSE(fs::exists(native_target));
            }
            // Preserve the components; computing fs::relative here could resolve
            // the link and accidentally hide the production bug. No cwd mutation.
            const auto path = (relative ? directory.filename() : directory) / "base" / "link" / ".." / "map.json";
            const auto origin = relative ? cwd / path : path;
            if (existing) {
                EXPECT_EQ(bytes(path), a_bytes); // Native lookup, before storage calls.
                EXPECT_EQ(MF::load(path), a);
                EXPECT_EQ(bytes(native_target), a_bytes);
                EXPECT_EQ(bytes(lexical_target), b_bytes);
            }
            MF::save(path, c);
            EXPECT_EQ(bytes(lexical_target), b_bytes);
            EXPECT_EQ(MF::load(lexical_target), b);
            ASSERT_TRUE(fs::exists(native_target));
            EXPECT_EQ(bytes(native_target), c_bytes);
            EXPECT_EQ(MF::load(native_target), c);
            EXPECT_EQ(MF::load(path), c);

            FaultOps ops;
            ops.fault = Fault::Replace;
            static_cast<void>(rejected([&] { MF::Detail::save(path, a, ops); },
                                      MF::Code::Replace, "save", origin, "$replace"));
            EXPECT_EQ(bytes(native_target), c_bytes);
            EXPECT_EQ(MF::load(native_target), c);
            EXPECT_EQ(bytes(lexical_target), b_bytes);
            EXPECT_EQ(MF::load(lexical_target), b);
            EXPECT_FALSE(fs::exists(fs::path(ops.staging_name.data())));
            EXPECT_EQ(std::distance(fs::directory_iterator(other), fs::directory_iterator{}), 2);
            EXPECT_EQ(std::distance(fs::directory_iterator(base), fs::directory_iterator{}), 2);
            EXPECT_EQ(fs::current_path(), cwd);
            EXPECT_EQ(bytes(unrelated), "unrelated staging-like bytes");
        }
    };

    TEST_F(files, ancestor_parent_absolute) { ancestor_parent(false, true); }
    TEST_F(files, ancestor_parent_relative) { ancestor_parent(true, true); }
    TEST_F(files, ancestor_parent_absolute_absent) { ancestor_parent(false, false); }
    TEST_F(files, ancestor_parent_relative_absent) { ancestor_parent(true, false); }

    TEST_F(files, round_trip) {
        const auto original = fixture();
        MF::save(target, original);
        auto restored = MF::load(target);
        EXPECT_EQ(restored, original); // Equality compares every field/cell/empty.
        EXPECT_EQ(bytes(target), MF::encode(original));
        restored.layers[0].cells[0] = "independent:edit";
        EXPECT_EQ(MF::load(target), original);
        EXPECT_NE(restored, original);
        EXPECT_EQ(entries().size(), 2u);
    }

    TEST_F(files, replacement) {
        MF::save(target, fixture());
        const auto old = bytes(target);
        MF::save(target, replacement());
        EXPECT_NE(bytes(target), old);
        EXPECT_EQ(bytes(target), MF::encode(replacement()));
        EXPECT_EQ(MF::load(target), replacement());
        EXPECT_EQ(entries().size(), 2u);
        EXPECT_EQ(bytes(unrelated), "unrelated staging-like bytes");
    }

    TEST_F(files, readonly_file_can_be_replaced) {
        MF::save(target, fixture());
        ASSERT_EQ(::chmod(target.c_str(), 0400), 0);
        MF::save(target, replacement()); // Directory permission controls rename.
        EXPECT_EQ(MF::load(target), replacement());
        struct stat state {};
        ASSERT_EQ(::stat(target.c_str(), &state), 0);
        EXPECT_EQ(state.st_mode & 0777, 0600u);
    }

    TEST_F(files, short_io_and_interrupts) {
        FaultOps writes;
        writes.short_writes = true;
        writes.interrupt_write = true;
        MF::Detail::save(target, fixture(), writes);
        EXPECT_GT(writes.write_calls, 2);
        EXPECT_EQ(writes.bytes_written, MF::encode(fixture()).size());
        EXPECT_EQ(writes.close_calls, 1);
        EXPECT_EQ(writes.replace_calls, 1);
        EXPECT_EQ(writes.native_replace_calls, 1);
        EXPECT_EQ(writes.remove_calls, 0); // No post-commit cleanup/syscall.
        EXPECT_EQ(fs::path(writes.staging_name.data()).parent_path(), directory);
        FaultOps reads;
        reads.short_reads = true;
        reads.interrupt_read = true;
        EXPECT_EQ(MF::Detail::load(target, reads), fixture());
        EXPECT_GT(reads.read_calls, 2);
        EXPECT_EQ(reads.close_calls, 1);
        EXPECT_EQ(reads.bytes_read, MF::encode(fixture()).size());
    }

    struct SaveFailure {
        const char* name;
        Fault fault;
        MF::Code code;
        const char* location;
        int number;
    };
    const std::array failures {
        SaveFailure{"create", Fault::Create, MF::Code::Open, "$create", EACCES},
        SaveFailure{"partial_write", Fault::PartialWrite, MF::Code::Write, "$write", ENOSPC},
        SaveFailure{"zero_write", Fault::ZeroWrite, MF::Code::Write, "$write", EIO},
        SaveFailure{"close", Fault::Close, MF::Code::Write, "$close", EINTR},
        SaveFailure{"replace", Fault::Replace, MF::Code::Replace, "$replace", EACCES},
        SaveFailure{"replace_interrupted", Fault::ReplaceInterrupted, MF::Code::Replace, "$replace", EINTR},
        SaveFailure{"cleanup", Fault::Cleanup, MF::Code::Replace, "$replace", EACCES}
    };
    class save_failure : public files, public testing::WithParamInterface<SaveFailure> {
    protected:
        void exercise(bool existing) {
            const auto b = replacement();
            if (existing)
                MF::save(target, fixture());
            const auto old = existing ? bytes(target) : std::string{};
            const auto prior_entries = entries();
            const auto& failure = GetParam();
            FaultOps ops;
            ops.fault = failure.fault;
            const auto error = rejected([&] { MF::Detail::save(target, b, ops); },
                                        failure.code, "save", target, failure.location);
            EXPECT_EQ(error.io_error, std::error_code(failure.number, std::generic_category()));
            EXPECT_EQ(ops.create_calls, 1);
            EXPECT_LE(ops.close_calls, 1);
            EXPECT_LE(ops.replace_calls, 1); // No automatic publication retries.
            EXPECT_EQ(ops.native_replace_calls, 0);
            if (failure.fault == Fault::PartialWrite) {
                EXPECT_GT(ops.bytes_written, 0u);
                EXPECT_LT(ops.bytes_written, MF::encode(b).size());
            }
            if (existing)
                intact(old);
            else
                EXPECT_FALSE(fs::exists(target));
            if (failure.fault == Fault::Cleanup) {
                EXPECT_EQ(error.cleanup_error, std::error_code(EBUSY, std::generic_category()));
                ASSERT_EQ(error.staging_leftover, ops.staging_name.data());
                EXPECT_EQ(bytes(error.staging_leftover), MF::encode(b));
                EXPECT_EQ(entries().size(), prior_entries.size() + 1);
                EXPECT_NE(std::string(error.what()).find("$replace"), std::string::npos);
            } else {
                EXPECT_TRUE(error.staging_leftover.empty());
                EXPECT_FALSE(error.cleanup_error);
                EXPECT_EQ(entries(), prior_entries);
            }
            MF::save(target, b); // Explicit caller retry after resolving the fault.
            EXPECT_EQ(MF::load(target), b);
            EXPECT_EQ(bytes(target), MF::encode(b));
            EXPECT_EQ(bytes(unrelated), "unrelated staging-like bytes");
            if (!error.staging_leftover.empty()) {
                EXPECT_TRUE(fs::exists(error.staging_leftover)); // No sweeping.
                EXPECT_TRUE(fs::remove(error.staging_leftover)); // Caller-owned cleanup.
            }
            EXPECT_EQ(entries().size(), 2u);
        }
    };
    TEST_P(save_failure, preserves_existing) { exercise(true); }
    TEST_P(save_failure, preserves_absent) { exercise(false); }
    INSTANTIATE_TEST_SUITE_P(files, save_failure, testing::ValuesIn(failures),
                            [](const testing::TestParamInfo<SaveFailure>& info) { return info.param.name; });

    TEST_F(files, native_rename_error_preserves_existing) {
        MF::save(target, fixture());
        const auto old = bytes(target);
        const auto prior_entries = entries();
        FaultOps ops;
        ops.fault = Fault::NativeRename;
        const auto error = rejected([&] { MF::Detail::save(target, replacement(), ops); },
                                    MF::Code::Replace, "save", target, "$replace");
        EXPECT_EQ(ops.native_replace_calls, 1); // Real rename against valid A.
        EXPECT_EQ(ops.native_replace_error, ENOENT);
        EXPECT_EQ(error.io_error.value(), ENOENT);
        EXPECT_TRUE(error.staging_leftover.empty()); // Already missing is clean.
        EXPECT_EQ(ops.close_calls, 1);
        EXPECT_EQ(ops.remove_calls, 1);
        intact(old);
        EXPECT_EQ(entries(), prior_entries);
        MF::save(target, replacement());
        EXPECT_EQ(MF::load(target), replacement());
    }

    TEST_F(files, exclusive_collision_is_not_owned) {
        MF::save(target, fixture());
        const auto old = bytes(target);
        FaultOps ops;
        ops.collision = directory / ".map-storage-COLLID";
        ASSERT_EQ(ops.collision.native().size(), (directory / ".map-storage-XXXXXX").native().size());
        put(ops.collision, "someone else's staging bytes");
        const auto prior_entries = entries();
        const auto error = rejected([&] { MF::Detail::save(target, replacement(), ops); },
                                    MF::Code::Open, "save", target, "$create");
        EXPECT_EQ(error.io_error.value(), EEXIST);
        EXPECT_EQ(ops.create_calls, 1);
        EXPECT_EQ(ops.close_calls, 0);
        EXPECT_EQ(ops.remove_calls, 0);
        EXPECT_EQ(bytes(ops.collision), "someone else's staging bytes");
        EXPECT_EQ(entries(), prior_entries);
        intact(old);
        MF::save(target, replacement());
        EXPECT_EQ(MF::load(target), replacement());
        EXPECT_EQ(bytes(ops.collision), "someone else's staging bytes");
    }

    TEST_F(files, staging_names_cannot_equal_absent_target) {
        for (const auto* name : {".map-storage-ABCDEF", ".map-staging-ABCDEF"}) {
            const auto path = directory / name;
            FaultOps ops;
            ops.fault = Fault::PartialWrite;
            static_cast<void>(rejected([&] { MF::Detail::save(path, fixture(), ops); },
                                      MF::Code::Write, "save", path, "$write"));
            const fs::path staging(ops.staging_name.data());
            EXPECT_NE(staging, path);
            auto prefix = staging.filename().native();
            prefix.resize(prefix.size() - 6); // mkostemp's variable suffix.
            EXPECT_FALSE(path.filename().native().starts_with(prefix));
            EXPECT_FALSE(fs::exists(path));
            EXPECT_FALSE(fs::exists(staging));
            MF::save(path, fixture());
            EXPECT_EQ(MF::load(path), fixture());
        }
        EXPECT_EQ(bytes(unrelated), "unrelated staging-like bytes");
    }

    TEST_F(files, close_does_not_retry_reused_descriptor) {
        MF::save(target, fixture());
        const auto old = bytes(target);
        FaultOps ops;
        ops.fault = Fault::Close;
        ops.reuse_after_close = unrelated;
        const auto error = rejected([&] { MF::Detail::save(target, replacement(), ops); },
                                    MF::Code::Write, "save", target, "$close");
        EXPECT_EQ(error.io_error.value(), EINTR);
        EXPECT_EQ(ops.close_calls, 1);
        ASSERT_GE(ops.reused_fd, 0);
        EXPECT_NE(::fcntl(ops.reused_fd, F_GETFD), -1);
        EXPECT_EQ(ops.replace_calls, 0);
        intact(old);
        EXPECT_EQ(entries().size(), 2u);
    }

    TEST_F(files, native_read_error_is_not_eof) {
        MF::save(target, fixture());
        const auto old = bytes(target);
        auto current = replacement();
        const auto prior = current;
        FaultOps ops;
        ops.read_failure_after = old.size(); // Entire valid JSON, then EIO instead of EOF.
        const auto error = rejected([&] { current = MF::Detail::load(target, ops); },
                                    MF::Code::Read, "parse", target, "$");
        EXPECT_EQ(error.io_error.value(), EIO);
        ASSERT_TRUE(error.byte_offset);
        EXPECT_EQ(*error.byte_offset, old.size() + 1);
        EXPECT_EQ(current, prior);
        EXPECT_EQ(ops.close_calls, 1);
        intact(old);
    }

    TEST_F(files, load_close_failure_preserves_value) {
        MF::save(target, fixture());
        const auto old = bytes(target);
        auto current = replacement();
        const auto prior = current;
        FaultOps ops;
        ops.fault = Fault::Close;
        ops.reuse_after_close = unrelated;
        const auto error = rejected([&] { current = MF::Detail::load(target, ops); },
                                    MF::Code::Read, "load", target, "$close");
        EXPECT_EQ(error.io_error.value(), EINTR);
        EXPECT_EQ(ops.close_calls, 1);
        EXPECT_EQ(current, prior);
        ASSERT_GE(ops.reused_fd, 0);
        EXPECT_NE(::fcntl(ops.reused_fd, F_GETFD), -1);
        intact(old);
    }

    TEST_F(files, open_and_inspect_failures) {
        MF::save(target, fixture());
        const auto old = bytes(target);
        FaultOps opens;
        opens.fail_open = true;
        const auto open_error = rejected([&] { static_cast<void>(MF::Detail::load(target, opens)); },
                                         MF::Code::Open, "load", target, "$open");
        EXPECT_EQ(open_error.io_error.value(), EACCES);
        EXPECT_EQ(opens.close_calls, 0);
        FaultOps inspect;
        inspect.fail_inspect = true;
        const auto inspect_error = rejected([&] { static_cast<void>(MF::Detail::load(target, inspect)); },
                                            MF::Code::Read, "load", target, "$open");
        EXPECT_EQ(inspect_error.io_error.value(), EIO);
        EXPECT_EQ(inspect.close_calls, 1);
        intact(old);
    }

    TEST_F(files, input_byte_budget) {
        auto document = MF::encode(fixture());
        document.resize(MF::max_document_bytes, ' ');
        put(target, document);
        FaultOps exact;
        EXPECT_EQ(MF::Detail::load(target, exact), fixture());
        EXPECT_EQ(exact.bytes_read, MF::max_document_bytes);
        document.append(5000, ' ');
        put(target, document);
        FaultOps over;
        const auto error = rejected([&] { static_cast<void>(MF::Detail::load(target, over)); },
                                    MF::Code::LimitExceeded, "parse", target, "$");
        EXPECT_EQ(over.bytes_read, MF::max_document_bytes + 1);
        ASSERT_TRUE(error.byte_offset);
        EXPECT_EQ(*error.byte_offset, MF::max_document_bytes + 1);
        EXPECT_EQ(over.close_calls, 1);
        EXPECT_EQ(bytes(target), document);
    }

    TEST_F(files, malformed_file_preserves_snapshot_and_bytes) {
        const auto a = MF::encode(fixture());
        const std::array documents {a.substr(0, a.size() - 8), a + "junk", a + '\0', a + '\0' + "junk", a + '\0' + a};
        for (const auto& document : documents) {
            put(target, document);
            auto current = fixture();
            const auto prior = current;
            const auto error = rejected([&] { current = MF::load(target); }, MF::Code::Malformed, "parse", target, "$");
            EXPECT_TRUE(error.byte_offset);
            EXPECT_EQ(current, prior);
            EXPECT_EQ(bytes(target), document);
        }
    }

    TEST_F(files, validates_before_staging) {
        MF::save(target, fixture());
        const auto old = bytes(target);
        const auto prior_entries = entries();
        auto invalid = replacement();
        invalid.layers[0].cells.pop_back();
        FaultOps ops;
        static_cast<void>(rejected([&] { MF::Detail::save(target, invalid, ops); },
                                  MF::Code::InvalidValue, "encode", "<snapshot>", "$.layers[0].cells"));
        EXPECT_EQ(ops.create_calls, 0);
        EXPECT_EQ(ops.status_calls, 0);
        intact(old);
        auto large = fixture();
        large.width = 256;
        large.height = 256;
        large.layers.resize(1);
        large.layers[0].cells.assign(65536, "fixture:" + std::string(120, 'x'));
        static_cast<void>(rejected([&] { MF::Detail::save(target, large, ops); },
                                  MF::Code::LimitExceeded, "encode", "<snapshot>", "$"));
        EXPECT_EQ(ops.create_calls, 0);
        intact(old);
        EXPECT_EQ(entries(), prior_entries);
    }

    TEST_F(files, embedded_nul_rejects_before_native_access) {
        MF::save(target, fixture());
        const auto old = bytes(target);
        auto raw = target.native();
        raw.push_back('\0');
        raw += "suffix";
        const fs::path bad(raw);
        FaultOps ops;
        static_cast<void>(rejected([&] { MF::Detail::save(bad, replacement(), ops); },
                                  MF::Code::InvalidValue, "save", bad, "$path"));
        static_cast<void>(rejected([&] { static_cast<void>(MF::Detail::load(bad, ops)); },
                                  MF::Code::InvalidValue, "load", bad, "$path"));
        EXPECT_EQ(ops.status_calls, 0);
        EXPECT_EQ(ops.create_calls, 0);
        EXPECT_EQ(ops.open_calls, 0);
        intact(old);
    }

    TEST_F(files, relative_path_and_owned_error) {
        const auto relative = fs::relative(target);
        MF::save(relative, fixture());
        EXPECT_EQ(MF::load(relative), fixture());
        MF::Error retained = [&] {
            FaultOps ops;
            ops.fault = Fault::Replace;
            fs::path temporary = relative;
            return rejected([&] { MF::Detail::save(temporary, replacement(), ops); },
                            MF::Code::Replace, "save", target, "$replace");
        }();
        EXPECT_EQ(retained.origin, target);
        EXPECT_EQ(retained.io_error.value(), EACCES);
        EXPECT_NE(std::string(retained.what()).find(target.string()), std::string::npos);
    }

    TEST_F(files, missing_and_invalid_paths) {
        const auto missing = target;
        const auto error = rejected([&] { static_cast<void>(MF::load(missing)); },
                                    MF::Code::Open, "load", missing, "$path");
        EXPECT_EQ(error.io_error.value(), ENOENT);
        const auto no_parent = directory / "missing" / "file.json";
        static_cast<void>(rejected([&] { MF::save(no_parent, fixture()); }, MF::Code::Open, "save", no_parent, "$path"));
        static_cast<void>(rejected([&] { MF::save({}, fixture()); }, MF::Code::InvalidValue, "save", {}, "$path"));
        static_cast<void>(rejected([&] { static_cast<void>(MF::load({})); }, MF::Code::InvalidValue, "load", {}, "$path"));
        EXPECT_FALSE(fs::exists(target));
        EXPECT_EQ(entries().size(), 1u);
    }

    TEST_F(files, rejects_symlinks_and_nonregular_entries) {
        const auto real = directory / "valid.map.json";
        MF::save(real, fixture());
        const auto original = bytes(real);
        const auto link = directory / "link.map.json";
        const auto dangling = directory / "dangling.map.json";
        fs::create_symlink(real, link);
        fs::create_symlink(target, dangling);
        const auto folder = directory / "folder";
        fs::create_directory(folder);
        const auto fifo = directory / "fifo";
        ASSERT_EQ(::mkfifo(fifo.c_str(), 0600), 0);
        const auto prior_entries = entries();
        for (const auto& path : {link, dangling, folder, fifo}) {
            static_cast<void>(rejected([&] { MF::save(path, replacement()); },
                                      MF::Code::InvalidValue, "save", path, "$path"));
            static_cast<void>(rejected([&] { static_cast<void>(MF::load(path)); },
                                      MF::Code::InvalidValue, "load", path, "$path"));
        }
        EXPECT_EQ(bytes(real), original);
        EXPECT_EQ(MF::load(real), fixture());
        EXPECT_TRUE(fs::is_symlink(link));
        EXPECT_TRUE(fs::is_symlink(dangling));
        EXPECT_EQ(entries(), prior_entries);
    }
}
#else
TEST(files, unsupported_platform) {
    const auto path = std::filesystem::current_path() / "unsupported-map-storage.json";
    try {
        CE::Maps::save(path, {});
        FAIL() << "unsupported save accepted";
    } catch (const CE::Maps::Error& error) {
        EXPECT_EQ(error.code, CE::Maps::Code::UnsupportedPlatform);
        EXPECT_EQ(error.operation, "save");
    }
    try {
        static_cast<void>(CE::Maps::load(path));
        FAIL() << "unsupported load accepted";
    } catch (const CE::Maps::Error& error) {
        EXPECT_EQ(error.code, CE::Maps::Code::UnsupportedPlatform);
        EXPECT_EQ(error.operation, "load");
    }
}
#endif
