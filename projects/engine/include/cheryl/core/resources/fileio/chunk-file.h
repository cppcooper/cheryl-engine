#pragma once
#include <internals/exceptions.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <iterator>
#include <memory>
#include <span>
#include <vector>

namespace CE::Files::Chunks {
    using FourCC = std::array<char, 4>;
    using Bytes = std::vector<std::byte>;
    enum class Category { data, container };
    enum class ErrorCode { format, limit, io };

    struct Limits {
        std::uint64_t max_bytes = 64 * 1024 * 1024;
        std::uint64_t max_allocation_bytes = 128 * 1024 * 1024;
        std::uint64_t max_chunks = 1'000'000;
        std::uint32_t max_depth = 64;
    };
    inline constexpr std::uint32_t hard_max_depth = 256;

    class Error : public Exceptions::runtime_exception {
        ErrorCode code_;
        std::uint64_t offset_;

    public:
        Error(ErrorCode code, std::uint64_t offset, const char* message);
        [[nodiscard]] ErrorCode code() const noexcept { return code_; }
        [[nodiscard]] std::uint64_t offset() const noexcept { return offset_; }
    };

    // Mutable writer values own payloads and children. DATA has no children;
    // CONT has no data. Encoding rejects values that violate this distinction.
    struct Chunk {
        Category category = Category::data;
        FourCC type{};
        Bytes data;
        std::vector<Chunk> children;

        [[nodiscard]] static Chunk leaf(FourCC type, Bytes data = {});
        [[nodiscard]] static Chunk container(FourCC type, std::vector<Chunk> children = {});
    };

    namespace Detail {
        struct Storage;
    }
    class ChunkRange;
    class Document;

    // Every view/range/iterator pins immutable storage. A returned span borrows
    // that storage: retain a handle while using it. No tree metadata is allocated.
    class ChunkView {
        std::shared_ptr<const Detail::Storage> storage_;
        std::uint64_t offset_;
        friend class ChunkRange;
        ChunkView(std::shared_ptr<const Detail::Storage> storage, std::uint64_t offset);

    public:
        [[nodiscard]] Category category() const;
        [[nodiscard]] FourCC type() const;
        [[nodiscard]] std::uint64_t offset() const noexcept { return offset_; }
        [[nodiscard]] std::span<const std::byte> payload() const;
        [[nodiscard]] ChunkRange children() const;
    };

    class ChunkRange {
        std::shared_ptr<const Detail::Storage> storage_;
        std::uint64_t begin_;
        std::uint64_t end_;
        friend class ChunkView;
        friend class Document;
        ChunkRange(std::shared_ptr<const Detail::Storage> storage, std::uint64_t begin, std::uint64_t end);

    public:
        class iterator {
            std::shared_ptr<const Detail::Storage> storage_;
            std::uint64_t position_ = 0;
            std::uint64_t end_ = 0;
            friend class ChunkRange;
            iterator(std::shared_ptr<const Detail::Storage> storage, std::uint64_t position, std::uint64_t end);

        public:
            using value_type = ChunkView;
            using difference_type = std::ptrdiff_t;
            using iterator_category = std::forward_iterator_tag;
            using iterator_concept = std::forward_iterator_tag;
            iterator() = default;
            [[nodiscard]] ChunkView operator*() const;
            iterator& operator++();
            iterator operator++(int);
            [[nodiscard]] bool operator==(const iterator&) const = default;
        };
        [[nodiscard]] iterator begin() const;
        [[nodiscard]] iterator end() const;
        [[nodiscard]] bool empty() const noexcept { return begin_ == end_; }
        [[nodiscard]] std::uint64_t size() const;
        [[nodiscard]] ChunkView at(std::uint64_t index) const;
        // Finds the nth matching application tag, retaining duplicate order.
        [[nodiscard]] ChunkView find(FourCC type, std::uint64_t occurrence = 0) const;
    };

    class Document {
        std::shared_ptr<const Detail::Storage> storage_;
        explicit Document(std::shared_ptr<const Detail::Storage> storage);
        friend Document decode(std::span<const std::byte>, const Limits&);
        friend Document read(std::istream&, std::uint64_t, const Limits&);

    public:
        [[nodiscard]] ChunkRange chunks() const;
        [[nodiscard]] std::span<const std::byte> bytes() const;
    };

    [[nodiscard]] Document decode(std::span<const std::byte> bytes, const Limits& limits = {});
    [[nodiscard]] Bytes encode(std::span<const Chunk> chunks, const Limits& limits = {});
    // Reads exactly byte_count bytes from the current stream position. Does not
    // consume subsequent bytes or close the caller's stream. Offset zero is here.
    [[nodiscard]] Document read(std::istream& stream, std::uint64_t byte_count, const Limits& limits = {});
    void write(std::ostream& stream, std::span<const Chunk> chunks, const Limits& limits = {});
    [[nodiscard]] Document read_file(const std::filesystem::path& path, const Limits& limits = {});
    // Validates before opening/truncating. Flushes and explicitly checks close.
    // Failure can leave a partial file; replacement is not atomic.
    void write_file(const std::filesystem::path& path, std::span<const Chunk> chunks, const Limits& limits = {});
}
