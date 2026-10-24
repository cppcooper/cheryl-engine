#pragma once

#include <cheryl/export.h>

#include <cheryl/core/resources/map-storage/snapshot.h>

#include <cstddef>
#include <filesystem>
#include <istream>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>

namespace CE::Maps {
    // Fixed v1 operational budgets, not a general configuration/result API.
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

    enum class Code {
        Read, Malformed, UnsupportedFormat, UnsupportedVersion, InvalidValue, LimitExceeded,
        Open, Write, Replace, UnsupportedPlatform
    };

    // All diagnostic data is owned, including when the source/stream has disappeared.
    // Printable diagnostics escape NUL in origin; the owned path remains exact.
    class CE_TYPE_API Error final : public std::runtime_error {
    public:
        Code code;
        std::string operation;
        std::filesystem::path origin;
        std::string location;
        std::optional<std::size_t> byte_offset; // One-based input position, when known.
        std::error_code io_error;
        // File-adapter cleanup never replaces the primary error. Empty unless an
        // owned staging entry could not be removed; the caller owns any retry.
        std::string staging_leftover;
        std::error_code cleanup_error;

        CE_API Error(Code code, std::string operation, std::filesystem::path origin, std::string location,
              const std::string& detail, std::optional<std::size_t> byte_offset = {}, std::error_code io_error = {});
    };

    // Parses cheryl.map-fixture integer version 1 only; JSON stays private.
    // Synchronous; borrows only during the call and returns independent owned values.
    // Consumes stream-buffer bytes, without changing the stream's flags/exception mask.
    // Reads at most max_document_bytes + one detection byte. Raw NUL always rejects.
    // Early limits/duplicates may precede later syntax errors. Anticipated JSON/I/O
    // failures throw Error; bad_alloc and programming exceptions propagate unchanged.
    // A streambuf reporting EOF is accepted as EOF; load() supplies checked native
    // reads. Origin is diagnostic only. The caller synchronizes shared input access.
    [[nodiscard]] CE_API Snapshot parse(std::istream& input, const std::filesystem::path& origin);

    // Revalidates aggregates and produces deterministic, bounded UTF-8 JSON with a
    // final LF. Failure never mutates the snapshot. No file or runtime state is used.
    // Borrows only during this synchronous call; concurrent input mutation requires
    // caller synchronization. Anticipated validation errors throw the owning Error.
    [[nodiscard]] CE_API std::string encode(const Snapshot& snapshot);

    // Checks complete semantics and current v1 encodability, including the 8 MiB
    // output cap, without materializing encoded bytes. Uses the same checks and
    // formatter as encode(); returns normally only for an encodable snapshot.
    // Synchronous, no mutation, I/O, asset lookup or retained references. Caller
    // synchronizes concurrent input mutation. Anticipated failures throw owning
    // Error with operation "validate", origin "<snapshot>" and the same category,
    // field and precedence as encode(); byte/I/O/cleanup context is absent.
    // Allocation/programming exceptions propagate unchanged; this is not noexcept
    // or an allocation-free operation (bounded semantic/path bookkeeping remains).
    CE_API void validate(const Snapshot& snapshot);
}
