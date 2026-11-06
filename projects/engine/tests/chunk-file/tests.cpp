#include <cheryl/core/resources/fileio/chunk-file.h>
#include "chunk-file-io.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <functional>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace C = CE::Files::Chunks;
namespace {
    constexpr C::FourCC root{'R', 'O', 'O', 'T'}, test{'T', 'E', 'S', 'T'};
    const C::Bytes payload{std::byte{0}, std::byte{0x41}, std::byte{0}};
    // Manually specified from the format contract, independent of encode().
    C::Bytes golden() {
        constexpr unsigned char bytes[]{0x43, 0x48, 0x4e, 0x4b, 0x0d, 0x0a, 0x1a, 0x0a, 0x01, 0x00, 0x00, 0x00, 0x10,
                                        0x00, 0x00, 0x00, 0x43, 0x4f, 0x4e, 0x54, 0x52, 0x4f, 0x4f, 0x54, 0x13, 0x00,
                                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x41, 0x54, 0x41, 0x54, 0x45, 0x53,
                                        0x54, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41, 0x00};
        C::Bytes result;
        for (auto byte : bytes)
            result.push_back(std::byte{byte});
        return result;
    }
    std::vector<C::Chunk> tree() {
        return {C::Chunk::container(root, {C::Chunk::leaf(test, payload)})};
    }
    void put_length(C::Bytes& bytes, std::size_t at, std::uint64_t n) {
        for (unsigned i = 0; i < 8; ++i)
            bytes[at + i] = std::byte((n >> (8 * i)) & 255);
    }
    void rejects(const std::function<void()>& action, C::ErrorCode code, std::uint64_t offset) {
        try {
            action();
            FAIL() << "expected chunk error";
        } catch (const C::Error& error) {
            EXPECT_EQ(error.code(), code);
            EXPECT_EQ(error.offset(), offset);
            EXPECT_NE(std::string(error.what()).find("byte " + std::to_string(offset)), std::string::npos);
        }
    }
    struct Temp {
        std::filesystem::path directory =
            std::filesystem::temp_directory_path() /
            ("cheryl-chunk-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Temp() {
            if (!std::filesystem::create_directory(directory))
                throw std::runtime_error("cannot create temporary directory");
        }
        ~Temp() {
            std::error_code ignored;
            std::filesystem::remove_all(directory, ignored);
        }
    };
    class FailingBuffer : public std::streambuf {
        bool flush_;
        bool throws_;

    public:
        FailingBuffer(bool flush, bool throws)
        : flush_(flush), throws_(throws) {}
        std::streamsize xsputn(const char*, std::streamsize n) override {
            if (flush_)
                return n;
            if (throws_)
                throw std::runtime_error("injected write failure");
            return n - 1;
        }
        int sync() override {
            if (throws_)
                throw std::runtime_error("injected flush failure");
            return -1;
        }
    };
    class BadRead : public std::streambuf {
        std::streamsize xsgetn(char*, std::streamsize) override { throw std::runtime_error("injected read failure"); }
    };
}

TEST(ChunkFile, ReadsFixture) {
    const auto bytes = golden();
    const auto document = C::decode(bytes);
    EXPECT_TRUE(std::ranges::equal(document.bytes(), bytes));
    ASSERT_EQ(document.chunks().size(), 1);
    const auto group = document.chunks().at(0);
    EXPECT_EQ(group.category(), C::Category::container);
    EXPECT_EQ(group.type(), root);
    EXPECT_EQ(group.offset(), 16);
    EXPECT_EQ(group.payload().size(), 19);
    ASSERT_EQ(group.children().size(), 1);
    const auto leaf = group.children().find(test);
    EXPECT_EQ(leaf.category(), C::Category::data);
    EXPECT_EQ(leaf.type(), test);
    EXPECT_EQ(leaf.offset(), 32);
    EXPECT_TRUE(std::ranges::equal(leaf.payload(), payload));
    EXPECT_TRUE(leaf.children().empty());
}
TEST(ChunkFile, WritesFixture) {
    EXPECT_EQ(C::encode(tree()), golden());
}
TEST(ChunkFile, Empty) {
    const auto bytes = golden();
    const auto empty = std::span<const std::byte>(bytes).first(16);
    EXPECT_TRUE(C::decode(empty).chunks().empty());
    EXPECT_TRUE(std::ranges::equal(C::encode({}), empty));
    rejects([] { static_cast<void>(C::decode({})); }, C::ErrorCode::format, 0);
    const std::vector values{C::Chunk::leaf(test), C::Chunk::container(root)};
    const auto document = C::decode(C::encode(values));
    ASSERT_EQ(document.chunks().size(), 2);
    EXPECT_TRUE(document.chunks().at(0).payload().empty());
    EXPECT_TRUE(document.chunks().at(1).children().empty());
}
TEST(ChunkFile, OrderAndTags) {
    const C::FourCC unknown{char(0), char(0xff), '?', ' '};
    const std::vector values{
        C::Chunk::container(
            root, {C::Chunk::leaf(unknown, {std::byte{1}}), C::Chunk::container(test), C::Chunk::leaf(unknown, payload),
                   C::Chunk::container(root, {C::Chunk::leaf(test)})}
        ),
        C::Chunk::leaf(test)
    };
    const auto document = C::decode(C::encode(values));
    ASSERT_EQ(document.chunks().size(), 2);
    const auto children = document.chunks().at(0).children();
    ASSERT_EQ(children.size(), 4);
    EXPECT_EQ(children.at(0).type(), unknown);
    EXPECT_EQ(children.at(1).type(), test);
    EXPECT_EQ(children.at(2).type(), unknown);
    EXPECT_EQ(children.at(3).children().at(0).type(), test);
    EXPECT_EQ(children.find(unknown).payload().size(), 1);
    EXPECT_TRUE(std::ranges::equal(children.find(unknown, 1).payload(), payload));
    EXPECT_THROW(static_cast<void>(children.find(unknown, 2)), std::out_of_range);
    EXPECT_THROW(static_cast<void>(children.at(4)), std::out_of_range);
    auto it = children.begin();
    auto previous = it++;
    EXPECT_EQ((*previous).type(), unknown);
    EXPECT_EQ((*it).type(), test);
    auto end = children.end();
    EXPECT_THROW(static_cast<void>(*end), std::out_of_range);
    EXPECT_THROW(++end, std::out_of_range);
}
TEST(ChunkFile, Envelope) {
    for (std::size_t i = 0; i < 16; ++i) {
        auto bytes = golden();
        bytes[i] ^= std::byte{1};
        rejects([&] { static_cast<void>(C::decode(bytes)); }, C::ErrorCode::format, i);
    }
    auto bytes = golden();
    bytes[16] = std::byte{'?'};
    rejects([&] { static_cast<void>(C::decode(bytes)); }, C::ErrorCode::format, 16);
}
TEST(ChunkFile, Truncation) {
    const auto bytes = golden();
    for (std::size_t n = 0; n < bytes.size(); ++n) {
        SCOPED_TRACE(n);
        if (n == 16)
            EXPECT_TRUE(C::decode(std::span<const std::byte>(bytes).first(n)).chunks().empty());
        else
            EXPECT_THROW(static_cast<void>(C::decode(std::span<const std::byte>(bytes).first(n))), C::Error);
    }
}
TEST(ChunkFile, Bounds) {
    auto bytes = golden();
    put_length(bytes, 40, 4);
    rejects([&] { static_cast<void>(C::decode(bytes)); }, C::ErrorCode::format, 40);
    bytes = golden();
    put_length(bytes, 24, 18);
    rejects([&] { static_cast<void>(C::decode(bytes)); }, C::ErrorCode::format, 40);
    bytes = golden();
    put_length(bytes, 24, 1);
    rejects([&] { static_cast<void>(C::decode(bytes)); }, C::ErrorCode::format, 32);
    bytes = golden();
    bytes.push_back(std::byte{0});
    rejects([&] { static_cast<void>(C::decode(bytes)); }, C::ErrorCode::format, 51);
    for (auto n : {std::uint64_t{1} << 32, std::numeric_limits<std::uint64_t>::max() - 16, std::numeric_limits<std::uint64_t>::max()}) {
        bytes = golden();
        put_length(bytes, 24, n);
        rejects([&] { static_cast<void>(C::decode(bytes)); }, C::ErrorCode::format, 24);
    }
    bytes = golden();
    put_length(bytes, 40, std::numeric_limits<std::uint64_t>::max());
    rejects([&] { static_cast<void>(C::decode(bytes)); }, C::ErrorCode::format, 40);
}
TEST(ChunkFile, ByteLimits) {
    C::Limits limits;
    limits.max_bytes = 50;
    rejects([&] { static_cast<void>(C::decode(golden(), limits)); }, C::ErrorCode::limit, 0);
    rejects([&] { static_cast<void>(C::encode(tree(), limits)); }, C::ErrorCode::limit, 0);
    limits.max_bytes = 51;
    EXPECT_EQ(C::encode(tree(), limits), golden());
    EXPECT_EQ(C::decode(golden(), limits).bytes().size(), 51);
    limits.max_bytes = 15;
    rejects([&] { static_cast<void>(C::encode({}, limits)); }, C::ErrorCode::limit, 0);
}
TEST(ChunkFile, AllocationLimits) {
    C::Limits limits;
    limits.max_allocation_bytes = 50;
    rejects([&] { static_cast<void>(C::encode(tree(), limits)); }, C::ErrorCode::limit, 0);
    limits.max_allocation_bytes = 51;
    EXPECT_EQ(C::encode(tree(), limits), golden());
    rejects([&] { static_cast<void>(C::decode(golden(), limits)); }, C::ErrorCode::limit, 0);
    // Discover the documented implementation-specific storage charge at its exact boundary.
    const auto charge = sizeof(C::Bytes) + 256;
    limits.max_allocation_bytes = 51 + charge - 1;
    rejects([&] { static_cast<void>(C::decode(golden(), limits)); }, C::ErrorCode::limit, 0);
    limits.max_allocation_bytes++;
    EXPECT_EQ(C::decode(golden(), limits).bytes().size(), 51);
}
TEST(ChunkFile, CountLimits) {
    C::Limits limits;
    limits.max_chunks = 1;
    rejects([&] { static_cast<void>(C::decode(golden(), limits)); }, C::ErrorCode::limit, 32);
    rejects([&] { static_cast<void>(C::encode(tree(), limits)); }, C::ErrorCode::limit, 32);
    limits.max_chunks = 2;
    EXPECT_EQ(C::encode(tree(), limits), golden());
    EXPECT_EQ(C::decode(golden(), limits).chunks().size(), 1);
    limits.max_chunks = 0;
    const auto bytes = golden();
    EXPECT_TRUE(C::decode(std::span<const std::byte>(bytes).first(16), limits).chunks().empty());
    EXPECT_EQ(C::encode({}, limits).size(), 16);
    const std::vector empty{C::Chunk::container(root)};
    rejects([&] { static_cast<void>(C::encode(empty, limits)); }, C::ErrorCode::limit, 16);
    rejects([&] { static_cast<void>(C::decode(C::encode(empty), limits)); }, C::ErrorCode::limit, 16);
}
TEST(ChunkFile, DepthLimits) {
    C::Limits limits;
    limits.max_depth = 1;
    rejects([&] { static_cast<void>(C::decode(golden(), limits)); }, C::ErrorCode::limit, 32);
    rejects([&] { static_cast<void>(C::encode(tree(), limits)); }, C::ErrorCode::limit, 32);
    limits.max_depth = 2;
    EXPECT_EQ(C::encode(tree(), limits), golden());
    EXPECT_EQ(C::decode(golden(), limits).chunks().size(), 1);
    limits.max_depth = 0;
    const auto bytes = golden();
    EXPECT_TRUE(C::decode(std::span<const std::byte>(bytes).first(16), limits).chunks().empty());
    EXPECT_EQ(C::encode({}, limits).size(), 16);
    rejects([&] { static_cast<void>(C::decode(bytes, limits)); }, C::ErrorCode::limit, 16);
    const std::vector empty{C::Chunk::container(root)};
    rejects([&] { static_cast<void>(C::encode(empty, limits)); }, C::ErrorCode::limit, 16);
    rejects([&] { static_cast<void>(C::decode(C::encode(empty), limits)); }, C::ErrorCode::limit, 16);
    limits.max_depth = C::hard_max_depth + 1;
    rejects([&] { static_cast<void>(C::encode({}, limits)); }, C::ErrorCode::limit, 0);
    rejects([&] { static_cast<void>(C::decode(bytes, limits)); }, C::ErrorCode::limit, 0);
}
TEST(ChunkFile, Deep) {
    // Build iteratively, including an empty deepest container. Codec walks never recurse.
    std::vector<C::Chunk> values{C::Chunk::container(root)};
    for (std::uint32_t i = 1; i < C::hard_max_depth; ++i)
        values = {C::Chunk::container(root, std::move(values))};
    C::Limits limits;
    limits.max_depth = C::hard_max_depth;
    const auto bytes = C::encode(values, limits);
    EXPECT_EQ(C::decode(bytes, limits).chunks().size(), 1);
    --limits.max_depth;
    rejects([&] { static_cast<void>(C::encode(values, limits)); }, C::ErrorCode::limit, 16 * C::hard_max_depth);
    rejects([&] { static_cast<void>(C::decode(bytes, limits)); }, C::ErrorCode::limit, 16 * C::hard_max_depth);
}
TEST(ChunkFile, InvalidValues) {
    for (int kind = 0; kind < 3; ++kind) {
        auto values = tree();
        if (kind == 0)
            values[0].category = static_cast<C::Category>(17);
        else if (kind == 1)
            values[0].data = payload;
        else
            values[0].category = C::Category::data;
        rejects([&] { static_cast<void>(C::encode(values)); }, C::ErrorCode::format, 16);
    }
}
TEST(ChunkFile, ValueCopies) {
    auto values = tree();
    auto copied = values;
    copied[0].children[0].data[0] = std::byte{9};
    EXPECT_EQ(C::encode(values), golden());
    auto moved = std::move(values);
    EXPECT_EQ(C::encode(moved), golden());
    EXPECT_NE(C::encode(copied), golden());
}
TEST(ChunkFile, ViewLifetime) {
    auto bytes = golden();
    auto document = C::decode(bytes);
    auto copy = document;
    auto moved = std::move(document);
    EXPECT_TRUE(document.chunks().empty());
    EXPECT_TRUE(document.bytes().empty());
    EXPECT_EQ(copy.bytes().data(), moved.bytes().data());
    bytes.assign(bytes.size(), std::byte{0});
    EXPECT_TRUE(std::ranges::equal(moved.bytes(), golden()));
    const auto view = [] { return C::decode(golden()).chunks().at(0).children().at(0); }();
    const auto range = [] { return C::decode(golden()).chunks().at(0).children(); }();
    const auto iterator = [] { return C::decode(golden()).chunks().at(0).children().begin(); }();
    EXPECT_TRUE(std::ranges::equal(view.payload(), payload));
    EXPECT_EQ(range.find(test).type(), test);
    EXPECT_TRUE(std::ranges::equal((*iterator).payload(), payload));
    const auto view_copy = view;
    auto range_copy = range;
    auto range_move = std::move(range_copy);
    EXPECT_EQ(view_copy.type(), test);
    EXPECT_EQ(range_move.at(0).type(), test);
}
TEST(ChunkFile, Streams) {
    const auto bytes = golden();
    const std::string wire(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    std::istringstream input(wire + "extra", std::ios::binary);
    EXPECT_EQ(C::read(input, bytes.size()).bytes().size(), bytes.size());
    EXPECT_EQ(input.peek(), 'e');
    std::ostringstream output(std::ios::binary);
    C::write(output, tree());
    EXPECT_EQ(output.str(), wire);
}
TEST(ChunkFile, ReadFailures) {
    for (bool exceptions : {false, true}) {
        std::istringstream input("short");
        if (exceptions)
            input.exceptions(std::ios::badbit | std::ios::failbit);
        rejects([&] { static_cast<void>(C::read(input, 51)); }, C::ErrorCode::io, 5);
        BadRead buffer;
        std::istream bad(&buffer);
        if (exceptions)
            bad.exceptions(std::ios::badbit | std::ios::failbit);
        rejects([&] { static_cast<void>(C::read(bad, 51)); }, C::ErrorCode::io, 0);
    }
    std::istringstream untouched("x");
    rejects([&] { static_cast<void>(C::read(untouched, std::numeric_limits<std::uint64_t>::max())); }, C::ErrorCode::limit, 0);
    EXPECT_EQ(untouched.tellg(), 0);
    rejects([&] { static_cast<void>(C::read(untouched, 0)); }, C::ErrorCode::format, 0);
    C::Limits limits;
    limits.max_allocation_bytes = 0;
    rejects([&] { static_cast<void>(C::read(untouched, 51, limits)); }, C::ErrorCode::limit, 0);
    EXPECT_EQ(untouched.tellg(), 0);
}
TEST(ChunkFile, WriteFailures) {
    for (bool flush : {false, true})
        for (bool throws : {false, true})
            for (bool exceptions : {false, true}) {
                FailingBuffer buffer(flush, throws);
                std::ostream output(&buffer);
                if (exceptions)
                    output.exceptions(std::ios::badbit | std::ios::failbit);
                rejects([&] { C::write(output, tree()); }, C::ErrorCode::io, flush ? 51 : 0);
            }
    std::ostringstream output;
    C::Limits limits;
    limits.max_chunks = 0;
    rejects([&] { C::write(output, tree(), limits); }, C::ErrorCode::limit, 16);
    EXPECT_TRUE(output.str().empty());
}
TEST(ChunkFile, CloseFailures) {
    struct File {
        bool throws;
        bool failed = false;
        void close() {
            failed = true;
            if (throws)
                throw std::ios_base::failure("injected close failure");
        }
        bool fail() const { return failed; }
    };
    for (bool throws : {false, true}) {
        File file{throws};
        rejects([&] { C::Detail::close_checked(file, 51); }, C::ErrorCode::io, 51);
    }
}
TEST(ChunkFile, Files) {
    Temp temp;
    const auto path = temp.directory / "data.chnk";
    C::write_file(path, tree());
    EXPECT_EQ(std::filesystem::file_size(path), 51);
    const auto document = C::read_file(path);
    EXPECT_TRUE(std::ranges::equal(document.bytes(), golden()));
    EXPECT_TRUE(std::ranges::equal(document.chunks().find(root).children().find(test).payload(), payload));
    C::Limits limits;
    limits.max_chunks = 0;
    rejects([&] { C::write_file(path, tree(), limits); }, C::ErrorCode::limit, 16);
    EXPECT_EQ(C::read_file(path).bytes().size(), 51); // Invalid input did not truncate the existing file.
    limits.max_bytes = 50;
    rejects([&] { static_cast<void>(C::read_file(path, limits)); }, C::ErrorCode::limit, 0);
    C::write_file(path, {});
    EXPECT_EQ(C::read_file(path).bytes().size(), 16);
    { std::ofstream empty(path, std::ios::binary | std::ios::trunc); }
    rejects([&] { static_cast<void>(C::read_file(path)); }, C::ErrorCode::format, 0);
    rejects([&] { static_cast<void>(C::read_file(temp.directory / "missing")); }, C::ErrorCode::io, 0);
    rejects([&] { C::write_file(temp.directory / "missing" / "data", tree()); }, C::ErrorCode::io, 0);
    rejects([&] { C::write_file(temp.directory, tree()); }, C::ErrorCode::io, 0);
}
