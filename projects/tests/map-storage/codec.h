#pragma once

#include "snapshot.h"

#include <cstddef>
#include <filesystem>
#include <istream>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>

namespace MapFixture {
    // Fixed private operational budgets, not a general configuration/result API.
    inline constexpr std::size_t max_document_bytes = 8 * 1024 * 1024;
    inline constexpr std::size_t max_depth = 16; // Root container has depth one.
    inline constexpr std::size_t max_key_bytes = 128;
    inline constexpr std::size_t max_dimension = 1024;
    inline constexpr std::size_t max_layers = 16;
    inline constexpr std::size_t max_spawns = 256;
    inline constexpr std::size_t max_cells = 262144;
    inline constexpr std::size_t max_array_elements = max_cells;
    inline constexpr std::size_t max_object_members = 256;
    inline constexpr std::size_t max_nodes = 300000; // Values/containers and decoded object keys.

    enum class Code { Read, Malformed, UnsupportedFormat, UnsupportedVersion, InvalidValue, LimitExceeded };

    // All diagnostic data is owned, including when the source/stream has disappeared.
    class Error final : public std::runtime_error {
    public:
        Code code;
        std::string operation;
        std::filesystem::path origin;
        std::string location;
        std::optional<std::size_t> byte_offset; // One-based input position, when known.
        std::error_code io_error;

        Error(Code code, std::string operation, std::filesystem::path origin, std::string location,
              const std::string& detail, std::optional<std::size_t> byte_offset = {}, std::error_code io_error = {});
    };

    // Synchronous; borrows only during the call and returns independent owned values.
    // Consumes stream-buffer bytes, without changing the stream's flags/exception mask.
    // Reads at most max_document_bytes + one detection byte. Raw NUL always rejects.
    // Early limits/duplicates may precede later syntax errors. Anticipated JSON/I/O
    // failures throw Error; bad_alloc and programming exceptions propagate unchanged.
    [[nodiscard]] Snapshot parse(std::istream& input, const std::filesystem::path& origin);

    // Revalidates aggregates and produces deterministic, bounded UTF-8 JSON with a
    // final LF. Failure never mutates the snapshot. No file or runtime state is used.
    [[nodiscard]] std::string encode(const Snapshot& snapshot);
}
