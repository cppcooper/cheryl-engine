#include <cheryl/tools/console/runtime/report.h>

#include <algorithm>
#include <charconv>
#include <stdexcept>
#include <system_error>
#include <utility>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <vector>
#endif
#if defined(__unix__) || defined(__APPLE__)
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace CE::Tools::Console::Runtime {
    namespace {
        void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
        bool component(std::string_view name) {
            if (name.empty() || name == "." || name == ".." || name.size() > 128) return false;
            for (const auto byte : name) if (!((byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
                (byte >= '0' && byte <= '9') || byte == '_' || byte == '-' || byte == '.')) return false;
            return true;
        }
#if defined(__unix__) || defined(__APPLE__)
        struct File {
            int fd = -1;
            explicit File(int value = -1) : fd(value) {}
            File(const File&) = delete;
            File& operator=(const File&) = delete;
            File(File&& other) noexcept : fd(std::exchange(other.fd, -1)) {}
            ~File() { if (fd >= 0) ::close(fd); }
        };
        constexpr int search_access =
#if defined(__linux__) && defined(O_PATH)
            O_PATH;
#elif defined(O_SEARCH)
            O_SEARCH;
#else
            O_RDONLY;
#endif
        File directory(const std::filesystem::path& root, bool readable = true) {
            require(root.is_absolute(), "Approved roots must be absolute");
            const auto relative = root.relative_path();
            File current(::open("/", (readable && relative.empty() ? O_RDONLY : search_access) |
                O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
            require(current.fd >= 0, "Cannot open root");
            for (auto part = relative.begin(); part != relative.end();) {
                auto name = part->string();
                ++part;
                require(component(name), "Unsafe root component");
                const auto access = readable && part == relative.end() ? O_RDONLY : search_access;
                const auto next = ::openat(current.fd, name.c_str(), access | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
                require(next >= 0, "Unsafe or missing root directory");
                ::close(current.fd); current.fd = next;
            }
            return current;
        }
        File open_relative(const std::filesystem::path& root, std::string_view relative) {
            require(!relative.empty() && relative.front() != '/' && relative.size() <= Core::max_text, "Unsafe relative path");
            auto current = directory(root, false);
            while (true) {
                const auto slash = relative.find('/');
                const auto name = relative.substr(0, slash);
                require(component(name), "Unsafe relative component");
                const auto flags = O_NOFOLLOW | O_CLOEXEC | (slash == std::string_view::npos ?
                    O_RDONLY | O_NONBLOCK : search_access | O_DIRECTORY);
                const auto next = ::openat(current.fd, std::string(name).c_str(), flags);
                require(next >= 0, "Unsafe or missing input");
                if (slash == std::string_view::npos) return File(next);
                ::close(current.fd); current.fd = next;
                relative.remove_prefix(slash + 1);
            }
        }
        void write_all(int fd, std::string_view bytes) {
            while (!bytes.empty()) {
                const auto written = ::write(fd, bytes.data(), bytes.size());
                if (written < 0 && errno == EINTR) continue;
                require(written > 0, "Report write failed");
                bytes.remove_prefix(static_cast<std::size_t>(written));
            }
        }
        void sync(int fd) { require(::fsync(fd) == 0, "Report sync failed"); }
#elif defined(_WIN32)
        struct File {
            HANDLE handle = INVALID_HANDLE_VALUE;
            explicit File(HANDLE value = INVALID_HANDLE_VALUE) : handle(value) {}
            File(const File&) = delete;
            File& operator=(const File&) = delete;
            File(File&& other) noexcept : handle(std::exchange(other.handle, INVALID_HANDLE_VALUE)) {}
            ~File() { close(); }
            void close() noexcept {
                if (handle != INVALID_HANDLE_VALUE) ::CloseHandle(std::exchange(handle, INVALID_HANDLE_VALUE));
            }
        };
        bool windows_component(std::string_view name) {
            if (!component(name) || name.back() == '.') return false;
            auto stem = std::string(name.substr(0, name.find('.')));
            for (auto& byte : stem) if (byte >= 'a' && byte <= 'z') byte -= 'a' - 'A';
            if (stem == "CON" || stem == "PRN" || stem == "AUX" || stem == "NUL") return false;
            return !(stem.size() == 4 && (stem.starts_with("COM") || stem.starts_with("LPT")) &&
                stem[3] >= '1' && stem[3] <= '9');
        }
        BY_HANDLE_FILE_INFORMATION information(const File& file, bool directory) {
            BY_HANDLE_FILE_INFORMATION info{};
            require(file.handle != INVALID_HANDLE_VALUE && ::GetFileType(file.handle) == FILE_TYPE_DISK &&
                ::GetFileInformationByHandle(file.handle, &info), "Cannot inspect confined file");
            require(!(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
                bool(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == directory, "Unsafe file type");
            return info;
        }
        struct Directory {
            std::filesystem::path path;
            std::vector<File> ancestors;
            void append(std::string_view name) {
                require(windows_component(name), "Unsafe Windows component");
                path /= std::string(name);
                File next(::CreateFileW(path.c_str(), FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES,
                    FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
                static_cast<void>(information(next, true));
                ancestors.push_back(std::move(next));
            }
        };
        Directory directory(const std::filesystem::path& root) {
            const auto drive = root.root_name().wstring();
            require(root.is_absolute() && drive.size() == 2 && drive[1] == L':' &&
                ((drive[0] >= L'A' && drive[0] <= L'Z') || (drive[0] >= L'a' && drive[0] <= L'z')),
                "Approved Windows roots require an absolute drive path");
            Directory current;
            current.path = root.root_path();
            File anchor(::CreateFileW(current.path.c_str(), FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES,
                FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
            static_cast<void>(information(anchor, true));
            current.ancestors.push_back(std::move(anchor));
            for (const auto& part : root.relative_path()) current.append(part.string());
            return current;
        }
        File create_output(const Directory& parent, const wchar_t* name) {
            const auto path = parent.path / name;
            File file(::CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW,
                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
            require(information(file, false).nNumberOfLinks == 1, "Output is not a private regular file");
            return file;
        }
        void write_all(HANDLE handle, std::string_view bytes) {
            while (!bytes.empty()) {
                const auto chunk = static_cast<DWORD>(std::min<std::size_t>(bytes.size(), 65536));
                DWORD written = 0;
                require(::WriteFile(handle, bytes.data(), chunk, &written, nullptr) && written > 0,
                    "Report write failed");
                bytes.remove_prefix(written);
            }
        }
        void sync(HANDLE handle) { require(::FlushFileBuffers(handle), "Report flush failed"); }
#endif
    }
    std::string content_id(std::string_view bytes) {
        std::uint64_t hash = 14695981039346656037ULL;
        for (unsigned char byte : bytes) { hash ^= byte; hash *= 1099511628211ULL; }
        char buffer[16];
        const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), hash, 16);
        return "fnv1a64:" + std::string(buffer, converted.ptr);
    }
    std::string quote(std::string_view text) {
        std::string result = "\"";
        constexpr char digits[] = "0123456789abcdef";
        for (unsigned char byte : text) {
            if (byte == '"' || byte == '\\') { result += '\\'; result += static_cast<char>(byte); }
            else if (byte < 32 || byte == 127) { result += "\\u00"; result += digits[byte >> 4]; result += digits[byte & 15]; }
            else result += static_cast<char>(byte);
        }
        return result + '"';
    }
    std::string typed(const Core::Value& value, const Core::Schema& schema) {
        std::string result = "{\"type\":" + std::to_string(static_cast<int>(value.type)) + ",\"value\":";
        if (value.type != schema.type) return result + "null,\"schema_mismatch\":true}";
        switch (value.type) {
        case Core::Type::Boolean: result += std::get<bool>(value.data) ? "true" : "false"; break;
        case Core::Type::Unsigned: result += std::to_string(std::get<std::uint64_t>(value.data)); break;
        case Core::Type::Integer: result += std::to_string(std::get<std::int64_t>(value.data)); break;
        case Core::Type::Scalar: {
            char buffer[64]; const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), std::get<double>(value.data));
            require(converted.ec == std::errc{}, "Cannot render scalar"); result.append(buffer, converted.ptr); break;
        }
        case Core::Type::Text: result += quote(std::get<std::string>(value.data)); break;
        case Core::Type::Enumeration: result += quote(schema.choices.at(std::get<Core::Enum>(value.data).index)); break;
        case Core::Type::Handle: {
            const auto& handle = std::get<Core::Handle>(value.data);
            result += "{\"domain\":" + quote(handle.domain()) + ",\"key\":" + std::to_string(handle.key()) + '}'; break;
        }
        case Core::Type::Record:
        case Core::Type::List: {
            const bool record = value.type == Core::Type::Record;
            result += record ? '{' : '[';
            const auto& children = std::get<Core::Value::Children>(value.data);
            for (std::size_t index = 0; index < children.size(); ++index) {
                if (index) result += ',';
                if (record) result += quote(schema.fields.at(index)) + ':';
                result += typed(children[index], schema.children.at(record ? index : 0));
            }
            result += record ? '}' : ']'; break;
        }
        }
        return result + '}';
    }
    std::string_view label(Script::Disposition disposition) noexcept {
        switch (disposition) {
        case Script::Disposition::Running: return "running";
        case Script::Disposition::Passed: return "passed";
        case Script::Disposition::Failed: return "failed";
        case Script::Disposition::Invalid: return "invalid";
        case Script::Disposition::Incomplete: return "incomplete";
        case Script::Disposition::Cancelled: return "cancelled";
        }
        return "incomplete";
    }
    namespace {
    std::string read_file(const std::filesystem::path& root, std::string_view relative, std::size_t limit) {
#if defined(__unix__) || defined(__APPLE__)
        auto file = open_relative(root, relative);
        struct stat info{};
        require(::fstat(file.fd, &info) == 0 && S_ISREG(info.st_mode) && info.st_nlink == 1 &&
            info.st_size >= 0 && static_cast<std::uint64_t>(info.st_size) <= limit, "Input is not a bounded private regular file");
        std::string bytes;
        bytes.reserve(static_cast<std::size_t>(info.st_size));
        char buffer[4096];
        while (true) {
            const auto count = ::read(file.fd, buffer, sizeof(buffer));
            if (count < 0 && errno == EINTR) continue;
            require(count >= 0, "Input read failed");
            if (!count) break;
            require(bytes.size() + static_cast<std::size_t>(count) <= limit, "Input grew beyond limit");
            bytes.append(buffer, static_cast<std::size_t>(count));
        }
        require(Core::valid_utf8(bytes), "Input is not UTF-8");
        return bytes;
#elif defined(_WIN32)
        require(!relative.empty() && relative.size() <= Core::max_text, "Unsafe relative path");
        auto parent = directory(root);
        while (relative.find('/') != std::string_view::npos) {
            const auto slash = relative.find('/');
            parent.append(relative.substr(0, slash));
            relative.remove_prefix(slash + 1);
        }
        require(windows_component(relative), "Unsafe relative component");
        const auto path = parent.path / std::string(relative);
        File file(::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
            FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
        const auto info = information(file, false);
        const auto size = (std::uint64_t(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
        require(info.nNumberOfLinks == 1 && size <= limit, "Input is not a bounded private regular file");
        std::string bytes;
        bytes.reserve(static_cast<std::size_t>(size));
        char buffer[4096];
        for (;;) {
            DWORD count = 0;
            require(::ReadFile(file.handle, buffer, sizeof(buffer), &count, nullptr), "Input read failed");
            if (!count) break;
            require(bytes.size() + count <= limit, "Input grew beyond limit");
            bytes.append(buffer, count);
        }
        require(Core::valid_utf8(bytes), "Input is not UTF-8");
        return bytes;
#else
        static_cast<void>(root); static_cast<void>(relative); static_cast<void>(limit);
        throw std::runtime_error("Scenario file confinement requires POSIX or Windows support");
#endif
    }
    }
    std::string read_source(const std::filesystem::path& root, std::string_view relative) {
        return read_file(root, relative, Script::max_file_bytes);
    }
    struct Report::State {
        std::string run;
        std::string transcript_bytes;
        std::size_t capacity = 0, written = 0;
#if defined(__unix__) || defined(__APPLE__)
        File directory, report, transcript;
#elif defined(_WIN32)
        Directory directory;
        File report, transcript;
#endif
        void charge(std::size_t size) {
            require(size <= capacity - written, "Scenario output capacity exhausted"); written += size;
        }
    };
    Report::Report(const std::filesystem::path& root, std::string run_id, std::size_t capacity)
        : state_(std::make_unique<State>()) {
        require(component(run_id) && run_id.size() <= 64, "Unsafe run identity");
        require(capacity >= 1024 && capacity <= max_output_bytes, "Invalid output capacity");
        state_->run = std::move(run_id); state_->capacity = capacity;
#if defined(__unix__) || defined(__APPLE__)
        auto parent = directory(root);
        require(::mkdirat(parent.fd, state_->run.c_str(), 0700) == 0, "Output run already exists or cannot be created");
        state_->directory.fd = ::openat(parent.fd, state_->run.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        require(state_->directory.fd >= 0, "Cannot open fresh output");
        state_->report.fd = ::openat(state_->directory.fd, "report.json", O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
        state_->transcript.fd = ::openat(state_->directory.fd, "transcript.txt", O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
        require(state_->report.fd >= 0 && state_->transcript.fd >= 0, "Cannot create fresh artifacts");
        sync(parent.fd);
#elif defined(_WIN32)
        require(windows_component(state_->run), "Unsafe Windows run identity");
        auto parent = directory(root);
        const auto path = parent.path / state_->run;
        require(::CreateDirectoryW(path.c_str(), nullptr), "Output run already exists or cannot be created");
        parent.append(state_->run);
        state_->directory = std::move(parent);
        auto report = create_output(state_->directory, L"report.json");
        auto transcript = create_output(state_->directory, L"transcript.txt");
        state_->report.handle = std::exchange(report.handle, INVALID_HANDLE_VALUE);
        state_->transcript.handle = std::exchange(transcript.handle, INVALID_HANDLE_VALUE);
#else
        static_cast<void>(root);
        throw std::runtime_error("Scenario output confinement requires POSIX or Windows support");
#endif
    }
    Report::~Report() = default;
    void Report::transcript(std::string_view bytes) {
        state_->charge(bytes.size());
        state_->transcript_bytes.append(bytes);
#if defined(__unix__) || defined(__APPLE__)
        write_all(state_->transcript.fd, bytes); sync(state_->transcript.fd);
#elif defined(_WIN32)
        write_all(state_->transcript.handle, bytes); sync(state_->transcript.handle);
#endif
    }
    void Report::running(std::string_view json) {
        require(json.size() <= Script::max_file_bytes, "Required report exceeds reader limit");
        state_->charge(json.size());
#if defined(__unix__) || defined(__APPLE__)
        require(::lseek(state_->report.fd, 0, SEEK_SET) == 0 && ::ftruncate(state_->report.fd, 0) == 0, "Cannot update owned report");
        write_all(state_->report.fd, json); sync(state_->report.fd);
#elif defined(_WIN32)
        LARGE_INTEGER beginning{};
        require(::SetFilePointerEx(state_->report.handle, beginning, nullptr, FILE_BEGIN) &&
            ::SetEndOfFile(state_->report.handle), "Cannot update owned report");
        write_all(state_->report.handle, json); sync(state_->report.handle);
#endif
    }
    void Report::finish(std::string_view json, std::string_view source_id, std::string_view build_id) {
        running(json);
#if defined(__unix__) || defined(__APPLE__)
        sync(state_->transcript.fd);
        File marker(::openat(state_->directory.fd, "complete", O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600));
        require(marker.fd >= 0, "Cannot create completion marker");
        try {
            const auto body = state_->run + '\n' + std::string(source_id) + '\n' + content_id(build_id) + '\n' +
                content_id(json) + '\n' + content_id(state_->transcript_bytes) + '\n';
            state_->charge(body.size()); write_all(marker.fd, body); sync(marker.fd); sync(state_->directory.fd);
        } catch (...) { invalidate(); throw; }
#elif defined(_WIN32)
        sync(state_->transcript.handle);
        const auto body = state_->run + '\n' + std::string(source_id) + '\n' + content_id(build_id) + '\n' +
            content_id(json) + '\n' + content_id(state_->transcript_bytes) + '\n';
        state_->charge(body.size());
        state_->report.close();
        state_->transcript.close();
        try {
            auto marker = create_output(state_->directory, L"complete");
            write_all(marker.handle, body); sync(marker.handle);
        } catch (...) { invalidate(); throw; }
#else
        static_cast<void>(source_id); static_cast<void>(build_id);
#endif
    }
    bool Report::invalidate() noexcept {
#if defined(__unix__) || defined(__APPLE__)
        if (state_->directory.fd >= 0) {
            if (::unlinkat(state_->directory.fd, "complete", 0) != 0 && errno != ENOENT) return false;
            return ::fsync(state_->directory.fd) == 0;
        }
#elif defined(_WIN32)
        if (!state_->directory.ancestors.empty()) {
            try {
                const auto marker = state_->directory.path / L"complete";
                if (::DeleteFileW(marker.c_str())) return true;
                return ::GetLastError() == ERROR_FILE_NOT_FOUND;
            } catch (...) { return false; }
        }
#endif
        return true;
    }
    const std::string& Report::run_id() const noexcept { return state_->run; }
    bool completed_report(const std::filesystem::path& root, std::string_view run_id, std::string_view source_id, std::string_view build_id) {
        try {
            if (!component(run_id)) return false;
            auto json = read_source(root, std::string(run_id) + "/report.json");
            auto marker = read_source(root, std::string(run_id) + "/complete");
            auto transcript = read_file(root, std::string(run_id) + "/transcript.txt", max_output_bytes);
            return marker == std::string(run_id) + '\n' + std::string(source_id) + '\n' + content_id(build_id) + '\n' +
                content_id(json) + '\n' + content_id(transcript) + '\n' &&
                json.starts_with("{\"schema\":1,\"final\":true,\"disposition\":\"passed\",");
        } catch (...) { return false; }
    }
}
