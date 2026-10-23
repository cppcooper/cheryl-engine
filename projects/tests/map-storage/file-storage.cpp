#include "file-storage.h"
#include "file-operations.h"

#if defined(__linux__)
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <streambuf>
#include <unistd.h>
#include <utility>

namespace MapFixture::Detail {
    int FileOps::status(const char* path, struct stat* result, bool follow) noexcept {
        return follow ? ::stat(path, result) : ::lstat(path, result);
    }
    int FileOps::inspect(int fd, struct stat* result) noexcept { return ::fstat(fd, result); }
    int FileOps::open_read(const char* path) noexcept { return ::open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK); }
    ssize_t FileOps::read(int fd, void* data, std::size_t count) noexcept { return ::read(fd, data, count); }
    int FileOps::create(char* name_template) noexcept { return ::mkostemp(name_template, O_CLOEXEC); }
    ssize_t FileOps::write(int fd, const void* data, std::size_t count) noexcept { return ::write(fd, data, count); }
    int FileOps::close(int fd) noexcept { return ::close(fd); }
    int FileOps::replace(const char* from, const char* to) noexcept { return ::rename(from, to); }
    int FileOps::remove(const char* path) noexcept { return ::unlink(path); }

    namespace {
        namespace fs = std::filesystem;

        std::error_code system_error(int value) noexcept { return {value, std::generic_category()}; }

        [[noreturn]] void fail(Code code, const char* operation, const fs::path& origin,
                               const char* location, const char* detail, int value = 0) {
            throw Error(code, operation, origin, location, detail, {}, system_error(value));
        }

        fs::path resolve(const fs::path& input, const char* operation) {
            if (input.empty() || input.native().find('\0') != std::string::npos)
                fail(Code::InvalidValue, operation, input, "$path", "empty path or embedded NUL");
            std::error_code error;
            auto result = fs::absolute(input, error).lexically_normal();
            if (error)
                throw Error(Code::Open, operation, input, "$path", "cannot resolve absolute path", {}, error);
            if (result.filename().empty() || result.filename() == "." || result.filename() == "..")
                fail(Code::InvalidValue, operation, result, "$path", "path must name a file");
            return result;
        }

        void profile(const fs::path& path, const char* operation, bool allow_absent, FileOps& ops) {
            struct stat state {};
            if (ops.status(path.parent_path().c_str(), &state, true) != 0)
                fail(Code::Open, operation, path, "$path", "cannot inspect parent directory", errno);
            if (!S_ISDIR(state.st_mode))
                fail(Code::InvalidValue, operation, path, "$path", "parent is not a directory");
            if (ops.status(path.c_str(), &state, false) != 0) {
                const auto error = errno;
                if (allow_absent && error == ENOENT)
                    return;
                fail(Code::Open, operation, path, "$path", "cannot inspect target", error);
            }
            if (!S_ISREG(state.st_mode))
                fail(Code::InvalidValue, operation, path, "$path", "target must be regular, without a final symlink");
        }

        struct Descriptor {
            FileOps& ops;
            int fd = -1;

            Descriptor(FileOps& ops, int fd) : ops(ops), fd(fd) {}
            Descriptor(const Descriptor&) = delete;
            Descriptor& operator=(const Descriptor&) = delete;
            ~Descriptor() {
                if (fd >= 0)
                    static_cast<void>(close());
            }

            int close() noexcept {
                // Linux may already release even an unsuccessfully closed fd.
                // Relinquish ownership first; never retry against a reused fd.
                const auto closing = std::exchange(fd, -1);
                return ops.close(closing);
            }
        };

        class ReadBuffer final : public std::streambuf {
            FileOps& ops;
            int fd;
            std::array<char, 4096> bytes {};
            std::size_t fetched = 0;

        public:
            ReadBuffer(FileOps& ops, int fd) : ops(ops), fd(fd) {}

        private:
            int_type underflow() override {
                if (gptr() && gptr() < egptr())
                    return traits_type::to_int_type(*gptr());
                const auto count = std::min(bytes.size(), max_document_bytes + 1 - fetched);
                if (count == 0)
                    return traits_type::eof(); // parse rejects at the detection byte.
                ssize_t got;
                do {
                    got = ops.read(fd, bytes.data(), count);
                } while (got < 0 && errno == EINTR);
                if (got < 0)
                    throw std::ios_base::failure("native file read failed", system_error(errno));
                if (got == 0)
                    return traits_type::eof();
                fetched += static_cast<std::size_t>(got);
                setg(bytes.data(), bytes.data(), bytes.data() + got);
                return traits_type::to_int_type(*gptr());
            }
        };

        struct Staging {
            FileOps& ops;
            std::string name;
            Descriptor handle;
            bool owned = false;

            // The staging prefix cannot name the target, even for an absent
            // target that already uses one of these temporary-looking names.
            Staging(FileOps& ops, const fs::path& target)
                : ops(ops),
                  name((target.parent_path() / (target.filename().native().starts_with(".map-storage-")
                      ? ".map-staging-XXXXXX" : ".map-storage-XXXXXX")).native()),
                  handle(ops, -1) {}
            ~Staging() { cleanup(nullptr); }

            void cleanup(Error* primary) noexcept {
                if (handle.fd >= 0)
                    static_cast<void>(handle.close());
                if (!std::exchange(owned, false))
                    return;
                if (ops.remove(name.c_str()) != 0 && errno != ENOENT && primary) {
                    primary->cleanup_error = system_error(errno);
                    primary->staging_leftover = std::move(name);
                }
            }
        };
    }

    Snapshot load(const std::filesystem::path& input, FileOps& ops) {
        const auto path = resolve(input, "load");
        profile(path, "load", false, ops);
        Descriptor handle(ops, ops.open_read(path.c_str()));
        if (handle.fd < 0)
            fail(Code::Open, "load", path, "$open", "cannot open regular file", errno);
        struct stat state {};
        if (ops.inspect(handle.fd, &state) != 0)
            fail(Code::Read, "load", path, "$open", "cannot inspect opened file", errno);
        if (!S_ISREG(state.st_mode))
            fail(Code::InvalidValue, "load", path, "$open", "opened file is not regular");
        ReadBuffer buffer(ops, handle.fd);
        std::istream stream(&buffer);
        auto snapshot = parse(stream, path);
        if (handle.close() != 0)
            fail(Code::Read, "load", path, "$close", "cannot close input", errno);
        return snapshot;
    }

    void save(const std::filesystem::path& input, const Snapshot& snapshot, FileOps& ops) {
        const auto path = resolve(input, "save");
        const auto bytes = encode(snapshot);
        profile(path, "save", true, ops);
        Staging staging(ops, path);
        staging.handle.fd = ops.create(staging.name.data());
        if (staging.handle.fd < 0)
            fail(Code::Open, "save", path, "$create", "cannot exclusively create sibling staging file", errno);
        staging.owned = true;
        try {
            std::size_t written = 0;
            while (written < bytes.size()) {
                const auto count = ops.write(staging.handle.fd, bytes.data() + written, bytes.size() - written);
                if (count < 0) {
                    if (errno == EINTR)
                        continue;
                    fail(Code::Write, "save", path, "$write", "cannot write staging file", errno);
                }
                if (count == 0)
                    fail(Code::Write, "save", path, "$write", "zero-progress staging write", EIO);
                written += static_cast<std::size_t>(count);
            }
            if (staging.handle.close() != 0)
                fail(Code::Write, "save", path, "$close", "cannot close staging file", errno);
            if (ops.replace(staging.name.c_str(), path.c_str()) != 0)
                fail(Code::Replace, "save", path, "$replace", "cannot replace target", errno);
            // Commit point passed: no more fallible work, including cleanup.
            staging.owned = false;
        } catch (Error& error) {
            staging.cleanup(&error);
            throw;
        }
        // Unexpected exceptions unwind through Staging without translation.
    }
}
#endif

namespace MapFixture {
    Snapshot load(const std::filesystem::path& path) {
#if defined(__linux__)
        Detail::FileOps ops;
        return Detail::load(path, ops);
#else
        throw Error(Code::UnsupportedPlatform, "load", path, "$platform", "file adapter requires Linux");
#endif
    }

    void save(const std::filesystem::path& path, const Snapshot& snapshot) {
#if defined(__linux__)
        Detail::FileOps ops;
        Detail::save(path, snapshot, ops);
#else
        static_cast<void>(snapshot);
        throw Error(Code::UnsupportedPlatform, "save", path, "$platform", "file adapter requires Linux");
#endif
    }
}
