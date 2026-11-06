#include <cheryl/core/resources/fileio/chunk-file.h>
#include "chunk-file-io.h"
#include <algorithm>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace CE::Files::Chunks {
    namespace Detail {
        struct Storage {
            Bytes bytes;
        };
    }
    namespace {
        constexpr std::array<std::byte, 16> envelope{std::byte{0x43}, std::byte{0x48}, std::byte{0x4e}, std::byte{0x4b},
                                                     std::byte{0x0d}, std::byte{0x0a}, std::byte{0x1a}, std::byte{0x0a},
                                                     std::byte{1},    std::byte{0},    std::byte{0},    std::byte{0},
                                                     std::byte{16},   std::byte{0},    std::byte{0},    std::byte{0}};
        constexpr FourCC cont{'C', 'O', 'N', 'T'}, data{'D', 'A', 'T', 'A'};
        constexpr std::uint64_t storage_charge = sizeof(Detail::Storage) + 256;
        constexpr std::uint64_t io_block = 1024 * 1024;
        [[noreturn]] void fail(ErrorCode code, std::uint64_t offset, const char* message) {
            throw Error(code, offset, message);
        }
        std::uint64_t add(std::uint64_t a, std::uint64_t b, std::uint64_t offset) {
            if (b > std::numeric_limits<std::uint64_t>::max() - a)
                fail(ErrorCode::format, offset, "length overflow");
            return a + b;
        }
        void options(const Limits& limits) {
            if (limits.max_depth > hard_max_depth)
                fail(ErrorCode::limit, 0, "depth exceeds implementation ceiling");
        }
        void budget(std::uint64_t size, const Limits& limits, std::uint64_t overhead) {
            if (size > limits.max_bytes || size > std::numeric_limits<std::size_t>::max() || size > Bytes{}.max_size())
                fail(ErrorCode::limit, 0, "byte budget exceeded");
            if (overhead > limits.max_allocation_bytes || size > limits.max_allocation_bytes - overhead)
                fail(ErrorCode::limit, 0, "allocation budget exceeded");
        }
        std::uint64_t length(std::span<const std::byte> bytes, std::uint64_t offset) {
            std::uint64_t result = 0;
            for (unsigned i = 0; i < 8; ++i)
                result |= std::uint64_t(std::to_integer<unsigned char>(bytes[static_cast<std::size_t>(offset + i)])) << (8 * i);
            return result;
        }
        FourCC tag(std::span<const std::byte> bytes, std::uint64_t offset) {
            FourCC result{};
            for (unsigned i = 0; i < 4; ++i)
                result[i] = static_cast<char>(std::to_integer<unsigned char>(bytes[static_cast<std::size_t>(offset + i)]));
            return result;
        }
        void put_length(Bytes& bytes, std::uint64_t offset, std::uint64_t value) {
            for (unsigned i = 0; i < 8; ++i)
                bytes[static_cast<std::size_t>(offset + i)] = std::byte((value >> (8 * i)) & 255);
        }
        void put_tag(Bytes& bytes, std::uint64_t offset, FourCC value) {
            for (unsigned i = 0; i < 4; ++i)
                bytes[static_cast<std::size_t>(offset + i)] = std::byte(static_cast<unsigned char>(value[i]));
        }
        void validate(std::span<const std::byte> bytes, const Limits& limits) {
            if (bytes.size() < envelope.size())
                fail(ErrorCode::format, bytes.size(), "truncated envelope");
            for (std::size_t i = 0; i < envelope.size(); ++i)
                if (bytes[i] != envelope[i])
                    fail(ErrorCode::format, i, "unsupported envelope");
            std::array<std::uint64_t, hard_max_depth + 1> ends{};
            ends[0] = bytes.size();
            std::uint32_t level = 0;
            std::uint64_t position = 16, count = 0;
            while (true) {
                if (position == ends[level]) {
                    if (!level)
                        break;
                    --level;
                    continue;
                }
                if (ends[level] - position < 16)
                    fail(ErrorCode::format, position, "incomplete chunk header");
                if (level >= limits.max_depth)
                    fail(ErrorCode::limit, position, "depth budget exceeded");
                if (count == limits.max_chunks)
                    fail(ErrorCode::limit, position, "chunk budget exceeded");
                ++count;
                const auto category = tag(bytes, position);
                if (category != data && category != cont)
                    fail(ErrorCode::format, position, "unknown structural category");
                const auto start = position + 16;
                const auto end = add(start, length(bytes, position + 8), position + 8);
                if (end > ends[level])
                    fail(ErrorCode::format, position + 8, "payload exceeds parent");
                if (category == cont) {
                    ends[++level] = end;
                    position = start;
                } else {
                    position = end;
                }
            }
        }
        struct Frame {
            std::span<const Chunk> chunks;
            std::size_t next = 0;
            std::uint64_t header = 0;
        };
        // The same bounded walk performs preflight and emission. Preflight has no
        // output pointer and allocates no metadata; emission cannot grow output.
        std::uint64_t walk(std::span<const Chunk> chunks, const Limits& limits, Bytes* output) {
            std::array<Frame, hard_max_depth + 1> frames{};
            frames[0].chunks = chunks;
            std::uint32_t level = 0;
            std::uint64_t position = 16, count = 0;
            budget(position, limits, 0);
            while (true) {
                auto& frame = frames[level];
                if (frame.next == frame.chunks.size()) {
                    if (!level)
                        break;
                    if (output)
                        put_length(*output, frame.header + 8, position - frame.header - 16);
                    --level;
                    continue;
                }
                if (level >= limits.max_depth)
                    fail(ErrorCode::limit, position, "depth budget exceeded");
                if (count == limits.max_chunks)
                    fail(ErrorCode::limit, position, "chunk budget exceeded");
                ++count;
                const auto& chunk = frame.chunks[frame.next++];
                if (chunk.category != Category::data && chunk.category != Category::container)
                    fail(ErrorCode::format, position, "invalid value category");
                if ((chunk.category == Category::data && !chunk.children.empty()) ||
                    (chunk.category == Category::container && !chunk.data.empty()))
                    fail(ErrorCode::format, position, "inconsistent chunk value");
                const auto header = position;
                position = add(position, 16, header);
                if (chunk.category == Category::data)
                    position = add(position, chunk.data.size(), header);
                budget(position, limits, 0);
                if (output) {
                    put_tag(*output, header, chunk.category == Category::data ? data : cont);
                    put_tag(*output, header + 4, chunk.type);
                    if (chunk.category == Category::data) {
                        put_length(*output, header + 8, chunk.data.size());
                        std::copy(chunk.data.begin(), chunk.data.end(), output->begin() + static_cast<std::ptrdiff_t>(header + 16));
                    }
                }
                if (chunk.category == Category::container)
                    frames[++level] = Frame{chunk.children, 0, header};
            }
            return position;
        }
        void write_bytes(std::ostream& stream, const Bytes& bytes) {
            std::uint64_t position = 0;
            while (position < bytes.size()) {
                const auto n = std::min<std::uint64_t>(io_block, bytes.size() - position);
                try {
                    stream.write(reinterpret_cast<const char*>(bytes.data() + position), static_cast<std::streamsize>(n));
                } catch (const std::exception&) {
                    fail(ErrorCode::io, position, "stream write failed");
                }
                if (!stream)
                    fail(ErrorCode::io, position, "stream write failed");
                position += n;
            }
            try {
                stream.flush();
            } catch (const std::exception&) {
                fail(ErrorCode::io, position, "stream flush failed");
            }
            if (!stream)
                fail(ErrorCode::io, position, "stream flush failed");
        }
    }
    Error::Error(ErrorCode code, std::uint64_t offset, const char* message)
    : Exceptions::runtime_exception(
          "chunk file",
          "CE::Files::Chunks",
          0,
          (std::string(message) + " at byte " + std::to_string(offset)).c_str()
      ),
      code_(code),
      offset_(offset) {}

    Chunk Chunk::leaf(FourCC type, Bytes bytes) {
        return {Category::data, type, std::move(bytes), {}};
    }
    Chunk Chunk::container(FourCC type, std::vector<Chunk> children) {
        return {Category::container, type, {}, std::move(children)};
    }
    ChunkView::ChunkView(std::shared_ptr<const Detail::Storage> storage, std::uint64_t offset)
    : storage_(std::move(storage)), offset_(offset) {}
    Category ChunkView::category() const {
        return tag(storage_->bytes, offset_) == cont ? Category::container : Category::data;
    }
    FourCC ChunkView::type() const {
        return tag(storage_->bytes, offset_ + 4);
    }
    std::span<const std::byte> ChunkView::payload() const {
        return std::span<const std::byte>(storage_->bytes)
            .subspan(static_cast<std::size_t>(offset_ + 16), static_cast<std::size_t>(length(storage_->bytes, offset_ + 8)));
    }
    ChunkRange ChunkView::children() const {
        const auto start = offset_ + 16;
        return {storage_, start, category() == Category::container ? start + length(storage_->bytes, offset_ + 8) : start};
    }
    ChunkRange::ChunkRange(std::shared_ptr<const Detail::Storage> storage, std::uint64_t begin, std::uint64_t end)
    : storage_(std::move(storage)), begin_(begin), end_(end) {}
    ChunkRange::iterator::iterator(std::shared_ptr<const Detail::Storage> storage, std::uint64_t position, std::uint64_t end)
    : storage_(std::move(storage)), position_(position), end_(end) {}
    ChunkView ChunkRange::iterator::operator*() const {
        if (position_ == end_)
            throw std::out_of_range("chunk iterator end");
        return {storage_, position_};
    }
    ChunkRange::iterator& ChunkRange::iterator::operator++() {
        if (position_ == end_)
            throw std::out_of_range("chunk iterator end");
        position_ += 16 + length(storage_->bytes, position_ + 8);
        return *this;
    }
    ChunkRange::iterator ChunkRange::iterator::operator++(int) {
        auto old = *this;
        ++*this;
        return old;
    }
    ChunkRange::iterator ChunkRange::begin() const {
        return {storage_, begin_, end_};
    }
    ChunkRange::iterator ChunkRange::end() const {
        return {storage_, end_, end_};
    }
    std::uint64_t ChunkRange::size() const {
        std::uint64_t n = 0;
        for (auto it = begin(); it != end(); ++it)
            ++n;
        return n;
    }
    ChunkView ChunkRange::at(std::uint64_t index) const {
        for (auto chunk : *this) {
            if (!index--)
                return chunk;
        }
        throw std::out_of_range("chunk index");
    }
    ChunkView ChunkRange::find(FourCC type, std::uint64_t occurrence) const {
        for (auto chunk : *this) {
            if (chunk.type() == type && !occurrence--)
                return chunk;
        }
        throw std::out_of_range("chunk tag");
    }
    Document::Document(std::shared_ptr<const Detail::Storage> storage)
    : storage_(std::move(storage)) {}
    ChunkRange Document::chunks() const {
        return {storage_, storage_ ? 16u : 0u, storage_ ? storage_->bytes.size() : 0u};
    }
    std::span<const std::byte> Document::bytes() const {
        return storage_ ? std::span<const std::byte>(storage_->bytes) : std::span<const std::byte>{};
    }
    Document decode(std::span<const std::byte> bytes, const Limits& limits) {
        options(limits);
        budget(bytes.size(), limits, storage_charge);
        validate(bytes, limits);
        auto storage = std::make_shared<Detail::Storage>();
        storage->bytes.assign(bytes.begin(), bytes.end());
        return Document(std::move(storage));
    }
    Bytes encode(std::span<const Chunk> chunks, const Limits& limits) {
        options(limits);
        const auto size = walk(chunks, limits, nullptr);
        Bytes bytes(static_cast<std::size_t>(size));
        std::copy(envelope.begin(), envelope.end(), bytes.begin());
        static_cast<void>(walk(chunks, limits, &bytes));
        return bytes;
    }
    Document read(std::istream& stream, std::uint64_t byte_count, const Limits& limits) {
        options(limits);
        budget(byte_count, limits, storage_charge);
        if (byte_count < 16)
            fail(ErrorCode::format, byte_count, "truncated envelope");
        auto storage = std::make_shared<Detail::Storage>();
        storage->bytes.resize(static_cast<std::size_t>(byte_count));
        std::uint64_t position = 0;
        while (position < byte_count) {
            const auto n = std::min(io_block, byte_count - position);
            bool threw = false;
            try {
                stream.read(reinterpret_cast<char*>(storage->bytes.data() + position), static_cast<std::streamsize>(n));
            } catch (const std::exception&) {
                threw = true;
            }
            const auto received = stream.gcount();
            if (threw || !stream || received != static_cast<std::streamsize>(n))
                fail(
                    ErrorCode::io, position + static_cast<std::uint64_t>(std::max<std::streamsize>(0, received)),
                    "stream read failed or truncated"
                );
            position += n;
        }
        validate(storage->bytes, limits);
        return Document(std::move(storage));
    }
    void write(std::ostream& stream, std::span<const Chunk> chunks, const Limits& limits) {
        write_bytes(stream, encode(chunks, limits));
    }
    Document read_file(const std::filesystem::path& path, const Limits& limits) {
        options(limits);
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
            fail(ErrorCode::io, 0, "file open failed");
        const auto end = file.tellg();
        if (end < std::streampos(0))
            fail(ErrorCode::io, 0, "file size query failed");
        const auto count = static_cast<std::uint64_t>(static_cast<std::streamoff>(end));
        budget(count, limits, storage_charge);
        file.seekg(0);
        if (!file)
            fail(ErrorCode::io, 0, "file seek failed");
        auto document = read(file, count, limits);
        // Detect growth between size query and read instead of silently accepting a prefix.
        if (file.peek() != std::char_traits<char>::eof() || file.bad())
            fail(ErrorCode::io, count, "file changed or final read failed");
        file.clear();
        Detail::close_checked(file, count);
        return document;
    }
    void write_file(const std::filesystem::path& path, std::span<const Chunk> chunks, const Limits& limits) {
        const auto bytes = encode(chunks, limits);
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file)
            fail(ErrorCode::io, 0, "file open failed");
        write_bytes(file, bytes);
        Detail::close_checked(file, bytes.size());
    }
}
