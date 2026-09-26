#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <type_traits>
#include <variant>

#include <assets/primitives/vertex.h>
#include <enums/fit-type.h>
#include <internals/exceptions.h>
#include <math/anchor.h>
#include <math/binary.h>
#include <math/bytes.h>
#include <math/fit.h>
#include <math/pointers.h>
#include <math/time.h>

// TODO(test-layout): assets/manifest.cpp currently owns the direct Anchor/MakePivot
// coverage. Once this file is integrated, remove that math-specific test from the
// asset manifest suite rather than maintaining duplicate coverage.
//
// TODO(test-layout): templates/block.cpp contains a direct get_alignment_offset()
// assertion. Its pointer helpers are otherwise mostly test setup for Block itself;
// move/remove only the math-specific assertion after this suite is established.

namespace {
    template <typename T>
    constexpr bool is_power_of_two(const T value) {
        return value != 0 && (value & (value - 1)) == 0;
    }
}

TEST(math_binary, selects_compact_storage_up_to_native_word) {
    using namespace CE::math::Detail;

    // Each width uses the smallest fixed-width integer available without
    // exceeding the platform's native word width.
    using expected_9 = std::conditional_t<(native_bits >= 16), std::uint16_t, native_word>;
    using expected_17 = std::conditional_t<(native_bits >= 32), std::uint32_t, native_word>;
    using expected_33 = std::conditional_t<(native_bits >= 64), std::uint64_t, native_word>;

    static_assert(std::is_same_v<bit_word_t<1>, std::uint8_t>);
    static_assert(std::is_same_v<bit_word_t<8>, std::uint8_t>);
    static_assert(std::is_same_v<bit_word_t<9>, expected_9>);
    static_assert(std::is_same_v<bit_word_t<17>, expected_17>);
    static_assert(std::is_same_v<bit_word_t<33>, expected_33>);

    // Crossing the native-word boundary changes storage from one scalar to
    // two native words without changing the BitArray interface.
    static_assert(bit_word_count<native_bits> == 1);
    static_assert(bit_word_count<native_bits + 1> == 2);

    EXPECT_EQ(sizeof(CE::math::BitArray<8>), sizeof(std::uint8_t));
    EXPECT_EQ(
        sizeof(CE::math::BitArray<native_bits + 1>),
        sizeof(native_word) * 2
    );
}

TEST(math_binary, reads_writes_and_copies_individual_bits) {
    CE::math::BitArray<8> bits;

    // Mutable indexing returns a proxy that writes through to the packed word.
    bits[0] = true;
    bits[3] = true;
    bits[7] = true;

    const auto& readable = bits;
    EXPECT_TRUE(readable[0]);
    EXPECT_FALSE(readable[1]);
    EXPECT_TRUE(readable[3]);
    EXPECT_TRUE(readable[7]);

    // Proxy assignment copies the represented bit value rather than rebinding
    // the temporary proxy to the source bit.
    bits[1] = bits[0];
    bits[2] = bits[4];

    EXPECT_TRUE(readable[1]);
    EXPECT_FALSE(readable[2]);

    // Explicit self-assignment must preserve the represented bit.
    auto reference = bits[3];
    reference = reference;
    EXPECT_TRUE(readable[3]);

    bits[3] = false;
    EXPECT_FALSE(readable[3]);
}

TEST(math_binary, crosses_native_word_boundaries) {
    constexpr std::size_t native_bits = CE::math::Detail::native_bits;
    CE::math::BitArray<native_bits + 3> bits;

    // Touch both sides of the first storage-word boundary and the final bit.
    bits[0] = true;
    bits[native_bits - 1] = true;
    bits[native_bits] = true;
    bits[native_bits + 2] = true;

    const auto& readable = bits;
    EXPECT_TRUE(readable[0]);
    EXPECT_TRUE(readable[native_bits - 1]);
    EXPECT_TRUE(readable[native_bits]);
    EXPECT_FALSE(readable[native_bits + 1]);
    EXPECT_TRUE(readable[native_bits + 2]);
}

TEST(math_binary, streams_bits_in_human_readable_order) {
    CE::math::BitArray<8> bits;
    bits[0] = true;
    bits[2] = true;
    bits[7] = true;

    // Stream output is most-significant to least-significant, while indexing
    // remains least-significant bit first.
    std::ostringstream out;
    out << bits;

    EXPECT_EQ(out.str(), "10000101");
}

TEST(math_anchor, resolves_named_anchors_and_pivots) {
    using namespace CE::math;

    struct AnchorCase {
        const char* name;
        AnchorType type;
        Pivot pivot;
    };

    constexpr std::array cases{
        AnchorCase{"TL", TopLeft, {0.0f, 0.0f}},
        AnchorCase{"TC", TopCenter, {0.5f, 0.0f}},
        AnchorCase{"TR", TopRight, {1.0f, 0.0f}},
        AnchorCase{"CL", CenterLeft, {0.0f, 0.5f}},
        AnchorCase{"ML", CenterLeft, {0.0f, 0.5f}},
        AnchorCase{"CR", CenterRight, {1.0f, 0.5f}},
        AnchorCase{"MR", CenterRight, {1.0f, 0.5f}},
        AnchorCase{"BL", BottomLeft, {0.0f, 1.0f}},
        AnchorCase{"BC", BottomCenter, {0.5f, 1.0f}},
        AnchorCase{"BR", BottomRight, {1.0f, 1.0f}}
    };

    // Names and aliases must resolve to the same normalized pivots used by the
    // geometry-building helpers.
    for (const auto& test : cases) {
        SCOPED_TRACE(test.name);
        EXPECT_EQ(get_anchor(test.name), test.type);
        EXPECT_EQ(get_pivot(test.type), test.pivot);
    }

    EXPECT_EQ(get_anchor("unknown"), Center);
    EXPECT_EQ(get_pivot(Center), (Pivot{0.5f, 0.5f}));
}

TEST(math_anchor, builds_matching_typed_and_float_geometry) {
    using namespace CE::math;

    constexpr Pivot pivot{0.5f, 1.0f};
    constexpr std::uint32_t texture_width = 64;
    constexpr std::uint32_t texture_height = 32;
    constexpr std::uint32_t width = 16;
    constexpr std::uint32_t height = 8;
    constexpr std::uint32_t x = 16;
    constexpr std::uint32_t y = 8;

    CE::Vertex2D typed[CE::VAONumbers::vertices_per_quad]{};
    std::array<float, CE::VAONumbers::floats_per_quad> raw{};

    // Both public overloads should describe the same two triangles.
    Anchor::MakePivot(pivot, typed, texture_width, texture_height, width, height, x, y);
    Anchor::MakePivot(pivot, raw.data(), texture_width, texture_height, width, height, x, y);

    EXPECT_FLOAT_EQ(typed[0].x, -8.0f);
    EXPECT_FLOAT_EQ(typed[0].y, 0.0f);
    EXPECT_FLOAT_EQ(typed[2].x, 8.0f);
    EXPECT_FLOAT_EQ(typed[2].y, 8.0f);
    EXPECT_FLOAT_EQ(typed[0].u, 0.25f);
    EXPECT_FLOAT_EQ(typed[0].v, 0.5f);
    EXPECT_FLOAT_EQ(typed[2].u, 0.5f);
    EXPECT_FLOAT_EQ(typed[2].v, 0.75f);

    for (std::size_t i = 0; i < CE::VAONumbers::vertices_per_quad; ++i) {
        const auto offset = i * CE::VAONumbers::floats_per_quad_vertex;
        EXPECT_FLOAT_EQ(raw[offset], typed[i].x);
        EXPECT_FLOAT_EQ(raw[offset + 1], typed[i].y);
        EXPECT_FLOAT_EQ(raw[offset + 2], typed[i].z);
        EXPECT_FLOAT_EQ(raw[offset + 3], typed[i].u);
        EXPECT_FLOAT_EQ(raw[offset + 4], typed[i].v);
    }

    // The two triangles deliberately repeat their shared diagonal.
    EXPECT_FLOAT_EQ(typed[0].x, typed[3].x);
    EXPECT_FLOAT_EQ(typed[0].y, typed[3].y);
    EXPECT_FLOAT_EQ(typed[2].x, typed[4].x);
    EXPECT_FLOAT_EQ(typed[2].y, typed[4].y);
}

TEST(math_anchor, rejects_invalid_pivots_and_texture_dimensions) {
    using namespace CE::math;

    std::array<float, CE::VAONumbers::floats_per_quad> vertices{};

    // Geometry generation requires a finite normalized pivot and a real texture
    // extent before any UV division can occur.
    EXPECT_THROW(
        Anchor::MakePivot(
            {std::numeric_limits<float>::quiet_NaN(), 0.5f},
            vertices.data(), 64, 32, 16, 8
        ),
        CE::Exceptions::invalid_args
    );
    EXPECT_THROW(
        Anchor::MakePivot({-0.01f, 0.5f}, vertices.data(), 64, 32, 16, 8),
        CE::Exceptions::invalid_args
    );
    EXPECT_THROW(
        Anchor::MakePivot({0.5f, 1.01f}, vertices.data(), 64, 32, 16, 8),
        CE::Exceptions::invalid_args
    );
    EXPECT_THROW(
        Anchor::MakePivot({0.5f, 0.5f}, vertices.data(), 0, 32, 16, 8),
        CE::Exceptions::invalid_args
    );
    EXPECT_THROW(
        Anchor::MakePivot({0.5f, 0.5f}, vertices.data(), 64, 0, 16, 8),
        CE::Exceptions::invalid_args
    );
}

TEST(math_pointers, offsets_and_ranges_use_byte_addresses) {
    std::array<std::byte, 128> memory{};
    auto* begin = memory.data();
    auto* end = begin + memory.size();

    // Ranges are inclusive at the beginning and exclusive at the end.
    EXPECT_TRUE(CE::ptr::is_in_range(begin, end, begin));
    EXPECT_TRUE(CE::ptr::is_in_range(begin, end, begin + 127));
    EXPECT_FALSE(CE::ptr::is_in_range(begin, end, end));

    // Offset helpers operate in bytes regardless of the eventual pointed-to type.
    EXPECT_EQ(
        CE::ptr::offset_address(begin, 17),
        reinterpret_cast<std::uintptr_t>(begin + 17)
    );
    EXPECT_EQ(CE::ptr::add_offset<std::byte>(begin, 17), begin + 17);
}

TEST(math_pointers, computes_alignment_and_alignment_offsets) {
    alignas(64) std::array<std::byte, 128> memory{};
    auto* begin = memory.data();

    // A deliberately aligned base needs no correction. Moving three bytes from
    // it requires thirteen more bytes to reach the next 16-byte boundary.
    EXPECT_EQ(
        CE::ptr::get_alignment_offset(begin, std::align_val_t{64}),
        std::size_t{0}
    );
    EXPECT_EQ(
        CE::ptr::get_alignment_offset(begin + 3, std::align_val_t{16}),
        std::size_t{13}
    );
    EXPECT_EQ(
        CE::ptr::align_offset(begin, 3, std::align_val_t{16}),
        std::size_t{16}
    );

    // calculate_alignment reports the largest power of two dividing an address.
    EXPECT_EQ(
        CE::ptr::calculate_alignment(std::uintptr_t{0x120}),
        std::size_t{32}
    );

    const auto actual_alignment =
        static_cast<std::size_t>(CE::ptr::calculate_alignment(begin));
    EXPECT_GE(actual_alignment, std::size_t{64});
    EXPECT_TRUE(is_power_of_two(actual_alignment));
}

TEST(math_pointers, hashes_pointers_into_requested_integer_widths) {
    int value = 0;
    void* pointer = &value;

    // The requested byte width controls the variant alternative while hashing
    // the same address remains deterministic.
    const auto hash8 = CE::ptr::pointer_to_hash(pointer, 1);
    const auto hash16 = CE::ptr::pointer_to_hash(pointer, 2);
    const auto hash32 = CE::ptr::pointer_to_hash(pointer, 4);
    const auto hash64 = CE::ptr::pointer_to_hash(pointer, 8);

    EXPECT_TRUE(std::holds_alternative<std::uint8_t>(hash8));
    EXPECT_TRUE(std::holds_alternative<std::uint16_t>(hash16));
    EXPECT_TRUE(std::holds_alternative<std::uint32_t>(hash32));
    EXPECT_TRUE(std::holds_alternative<std::uint64_t>(hash64));
    EXPECT_EQ(hash32, CE::ptr::pointer_to_hash(pointer, 4));

    EXPECT_THROW(
        static_cast<void>(CE::ptr::pointer_to_hash(pointer, 3)),
        CE::Exceptions::invalid_args
    );
}

TEST(math_fit, adjusts_lengths_and_reduces_fit_policy) {
    // exact preserves the request, larger adds fixed growth, and greedy applies
    // multiplicative growth before adding the same fixed amount.
    EXPECT_EQ(CE::Math::adjust_length(100, CE::Enum::exact, 8, 1.5), std::size_t{100});
    EXPECT_EQ(CE::Math::adjust_length(100, CE::Enum::larger, 8, 1.5), std::size_t{108});
    EXPECT_EQ(CE::Math::adjust_length(100, CE::Enum::greedy, 8, 1.5), std::size_t{158});

    EXPECT_EQ(CE::Math::reduce(CE::Enum::greedy), CE::Enum::larger);
    EXPECT_EQ(CE::Math::reduce(CE::Enum::larger), CE::Enum::exact);
    EXPECT_EQ(CE::Math::reduce(CE::Enum::exact), CE::Enum::exact);
}

TEST(math_time, identifies_and_casts_chrono_durations) {
    static_assert(is_chrono_duration_v<Milliseconds>);
    static_assert(is_chrono_duration_v<Seconds>);
    static_assert(!is_chrono_duration_v<int>);

    // duration_cast semantics intentionally truncate fractional destination units.
    const auto seconds = tcast<Milliseconds, Seconds>(Milliseconds{1500});
    EXPECT_EQ(seconds, Seconds{1});
    EXPECT_EQ(mscast(Seconds{2}), Milliseconds{2000});
    EXPECT_EQ(scast(Milliseconds{2500}), Seconds{2});
    EXPECT_EQ(mcast(Hours{2}), Minutes{120});
    EXPECT_EQ(hcast(Minutes{120}), Hours{2});
}

TEST(math_bytes, formats_common_binary_units) {
    // Exercise ordinary values without encoding the known exact-power boundary
    // behavior as the desired contract.
    EXPECT_EQ(human_readable(0), "0.0 bytes");
    EXPECT_EQ(human_readable(512), "512.0 bytes");
    EXPECT_EQ(human_readable(1536), "1.5KiB");
    EXPECT_EQ(human_readable(std::size_t{1536} * 1024), "1.5MiB");

    // TODO(math-bytes): Add exact powers-of-1024 and TiB-and-larger cases after
    // deciding/fixing the intended boundary and suffix behavior. The current
    // implementation uses `> 1024` and skips the TiB suffix.
}
