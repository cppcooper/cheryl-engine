#pragma once

#include "file-storage.h"

#if defined(__linux__)
#include <sys/stat.h>
#include <sys/types.h>

namespace MapFixture::Detail {
    // Narrow private syscall boundary for deterministic I/O-failure tests. The
    // defaults perform real Linux calls; no global fault state or SDK surface.
    struct FileOps {
        virtual ~FileOps() = default;
        virtual int status(const char* path, struct stat* result, bool follow) noexcept;
        virtual int inspect(int fd, struct stat* result) noexcept;
        virtual int open_read(const char* path) noexcept;
        virtual ssize_t read(int fd, void* data, std::size_t count) noexcept;
        virtual int create(char* name_template) noexcept;
        virtual ssize_t write(int fd, const void* data, std::size_t count) noexcept;
        virtual int close(int fd) noexcept;
        virtual int replace(const char* from, const char* to) noexcept;
        virtual int remove(const char* path) noexcept;
    };

    [[nodiscard]] Snapshot load(const std::filesystem::path& path, FileOps& ops);
    void save(const std::filesystem::path& path, const Snapshot& snapshot, FileOps& ops);
}
#endif
