#pragma once

#include <cheryl/core/resources/map-storage/codec.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Bounded CPU cell edits tied to full canonical base bytes; no patch wire format.
namespace CE::Maps::CellPatches {
    inline constexpr std::size_t max_base_cells = 65536;
    inline constexpr std::size_t max_edits = 1024;
    inline constexpr std::size_t max_witness_bytes = CE::Maps::max_document_bytes;
    static_assert(max_base_cells <= CE::Maps::max_cells);

    struct Edit {
        std::string layer;
        std::uint32_t x = 0;
        std::uint32_t y = 0;
        std::optional<std::string> tile; // nullopt clears; a key sets, including a no-op set.

        [[nodiscard]] bool operator==(const Edit&) const = default;
    };

    struct Patch;

    // capture()/diff() create valid witnesses: full validated canonical bytes, not
    // a compact or stable cross-version digest. Copies own their bytes. Default
    // and moved-from witnesses reject.
    class Witness {
        std::string bytes_;
        bool valid_ = false;

        explicit Witness(std::string bytes);
        friend Witness capture(const CE::Maps::Snapshot& base);
        friend CE::Maps::Snapshot apply(const CE::Maps::Snapshot& base, const Patch& patch);
        friend Patch diff(const CE::Maps::Snapshot& base, const CE::Maps::Snapshot& target);

    public:
        Witness() = default;
        Witness(const Witness&) = default;
        Witness& operator=(const Witness&) = default;
        CE_API Witness(Witness&& other) noexcept;
        CE_API Witness& operator=(Witness&& other) noexcept;

        // Borrowed view, invalidated by destruction, move or assignment of this
        // witness. Copying a witness owns bytes; copying this view does not.
        [[nodiscard]] CE_API std::string_view canonical_bytes() const noexcept;
        [[nodiscard]] bool operator==(const Witness&) const = default;
    };

    struct Patch {
        Witness witness;
        std::vector<Edit> edits;

        [[nodiscard]] bool operator==(const Patch&) const = default;
    };

    enum class Code {
        InvalidBase, InvalidWitness, BaseMismatch, InvalidEdit, LimitExceeded,
        InvalidTarget, UnsupportedChange
    };

    // All primary and optional codec context is owned and survives input lifetime.
    class CE_TYPE_API Error final : public std::runtime_error {
    public:
        Code code;
        std::string operation;
        std::string location;
        std::optional<CE::Maps::Error> cause; // Owned original codec context when available.

        CE_API Error(Code code, std::string operation, std::string location, const std::string& detail,
              std::optional<CE::Maps::Error> cause = {});
    };

    // Cheap shape/product admission precedes full public encode() validation.
    // Up to 65,536 cells and 8 MiB canonical bytes; no file access or asset lookup.
    [[nodiscard]] CE_API Witness capture(const CE::Maps::Snapshot& base);

    // After count/witness admission, re-encode and compare the ENTIRE base before
    // validating edit addresses/values. Only cell values change, addressed by
    // stable layer identity and zero-origin finite 2D coordinates.
    // At most 1,024 unique addresses; duplicate addresses reject even for no-ops.
    // Admit the batch/address/key-size bounds before one deep candidate copy;
    // public validate(candidate) then checks semantics and complete encoded size.
    // Invalid-key errors can follow allocation and use candidate cell order.
    // An empty batch still requires an exact match and returns an independent
    // copy. No input/result aliasing, mutation or I/O.
    // Anticipated rejections throw owning Error; bad_alloc/programming exceptions
    // propagate. Failures preserve all inputs and the caller's held result. Caller
    // synchronizes concurrent input mutation. Bounds describe logical storage,
    // not an exact allocator/heap byte quota; the full witness is not compact.
    [[nodiscard]] CE_API CE::Maps::Snapshot apply(const CE::Maps::Snapshot& base, const Patch& patch);

    // Prepare cell-only changes between two validated, patch-bounded snapshots.
    // Base validation/encoding precedes target admission/validate(), then exact
    // metadata checks: map ID, dimensions, ordered layer IDs and all spawns/order.
    // Emit changed optional keys only, in authored-layer then row-major order.
    // Count first; reject change 1,025 before allocating/copying an edit vector.
    // Equal values yield an empty patch still bound to the full original base.
    // Owns <=8 MiB witness bytes plus <=1,024 bounded edits, without a snapshot
    // copy or target output buffer; other scratch is bounded validation/path data.
    // No mutation, I/O or retained references. Caller synchronizes shared inputs.
    // Owning Error uses operation "diff", $.base/$.target context and an optional
    // original codec cause. InvalidTarget/UnsupportedChange distinguish invalid
    // target content from valid forbidden metadata changes; budget errors retain
    // LimitExceeded. Allocation/programming exceptions propagate unchanged, and
    // both inputs and a previously held result survive failure.
    [[nodiscard]] CE_API Patch diff(const CE::Maps::Snapshot& base, const CE::Maps::Snapshot& target);
}
