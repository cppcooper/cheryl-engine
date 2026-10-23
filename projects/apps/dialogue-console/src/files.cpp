#include "files.h"

#include <cerrno>
#include <cstdio>
#include <system_error>
#include <utility>

namespace GateConsole {
    namespace {
        bool valid_path(const std::string& path) {
            return !path.empty() && path.find('\0') == std::string::npos;
        }
        std::string io_error(const char* operation, const std::string& path, int code) {
            return std::string(operation) + " '" + path + "': " + std::error_code(code, std::generic_category()).message();
        }
    }

    std::expected<std::string, std::string> read_save(const std::string& path) {
        if (!valid_path(path))
            return std::unexpected("Save path is empty or contains NUL");
        // Allocate the fixed bound before acquiring the stream. A detection byte
        // proves oversize without unbounded reads or a size/check/read race.
        std::string bytes(maximum_save_bytes + 1, '\0');
        auto* stream = std::fopen(path.c_str(), "rb");
        if (!stream)
            return std::unexpected(io_error("Cannot read", path, errno));
        const auto count = std::fread(bytes.data(), 1, bytes.size(), stream);
        const bool failed = std::ferror(stream);
        const int read_error = errno;
        const int closed = std::fclose(stream);
        const int close_error = errno;
        if (failed)
            return std::unexpected(io_error("Read failed", path, read_error));
        if (closed)
            return std::unexpected(io_error("Read close failed", path, close_error));
        if (count > maximum_save_bytes)
            return std::unexpected("Save exceeds 64 KiB: " + path);
        bytes.resize(count);
        return bytes;
    }

    std::expected<void, std::string> save_fresh(Encounter& encounter, const std::string& path, Testing::FileFailures failures) {
        if (!valid_path(path))
            return std::unexpected("Save path is empty or contains NUL");
        auto paired = encounter.capture();
        if (!paired)
            return std::unexpected(std::string(describe(paired.error())));
        const auto bytes = encode(*paired);
        if (bytes.size() > maximum_save_bytes)
            return std::unexpected("Save exceeds 64 KiB");
        if (failures.open)
            return std::unexpected(io_error("Exclusive create failed", path, EIO));
        // C11's x mode exclusively creates a new file. Existing files and leaf
        // links fail at creation; there is no check-then-truncate or replacement.
        auto* stream = std::fopen(path.c_str(), "wbx");
        if (!stream)
            return std::unexpected(io_error("Exclusive create failed", path, errno));
        // Ownership begins here only. No throwing C++ work occurs until checked
        // close and (on failure) cleanup are complete.
        const char* failure = nullptr;
        int failure_code = 0;
        const auto requested = failures.partial_write ? bytes.size() / 2 : bytes.size();
        const auto written = std::fwrite(bytes.data(), 1, requested, stream);
        if (written != bytes.size()) {
            failure = "Write failed";
            failure_code = failures.partial_write ? EIO : errno;
        }
        const int flushed = std::fflush(stream);
        if (!failure && (flushed || failures.flush)) {
            failure = "Flush failed";
            failure_code = failures.flush ? EIO : errno;
        }
        const int closed = std::fclose(stream);
        if (!failure && (closed || failures.close)) {
            failure = "Close failed";
            failure_code = failures.close ? EIO : errno;
        }
        if (!failure)
            return {};
        const bool cleanup_failed = failures.cleanup || std::remove(path.c_str()) != 0;
        const int cleanup_code = failures.cleanup ? EIO : errno;
        auto message = io_error(failure, path, failure_code);
        if (cleanup_failed)
            message += "; " + io_error("Owned incomplete output cleanup failed", path, cleanup_code);
        return std::unexpected(std::move(message));
    }

    std::expected<void, std::string> resume_file(Encounter& encounter, const std::string& path) {
        auto bytes = read_save(path);
        if (!bytes)
            return std::unexpected(std::move(bytes.error()));
        return restore_bytes(encounter, *bytes);
    }
}
