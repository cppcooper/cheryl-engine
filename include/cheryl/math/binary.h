#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ostream>
#include <type_traits>

namespace CE::math {
    namespace Detail {
        // Use size_t as Cheryl's native storage word. Small bit arrays stay in the
        // narrowest fixed-width integer that can contain them; once Bits exceeds
        // the native word width, storage is split across native-word elements.
        using native_word = std::size_t;
        inline constexpr std::size_t native_bits =
            std::numeric_limits<native_word>::digits;

        // Select the smallest fixed-width integer that can hold Bits without
        // exceeding the native word size. If none qualify, use the native word.
        template <std::size_t Bits>
        using bit_word_t =
        std::conditional_t<Bits <= 8, std::uint8_t,
                           std::conditional_t<Bits <= 16 && native_bits >= 16, std::uint16_t,
                                              std::conditional_t<Bits <= 32 && native_bits >= 32, std::uint32_t,
                                                                 std::conditional_t<Bits <= 64 && native_bits >= 64, std::uint64_t,
                                                                                    native_word>>>>;

        // Width, in bits, of the selected backing word.
        template <std::size_t Bits>
        inline constexpr std::size_t bit_word_bits =
            std::numeric_limits<bit_word_t<Bits>>::digits;

        // Number of backing words required to store Bits.
        template <std::size_t Bits>
        inline constexpr std::size_t bit_word_count =
            (Bits + bit_word_bits<Bits> - 1) / bit_word_bits<Bits>;

        // Keep single-word bit arrays as a scalar; larger arrays become a packed
        // sequence of backing words.
        template <std::size_t Bits>
        using bit_storage_t =
        std::conditional_t<
            bit_word_count<Bits> == 1,
            bit_word_t<Bits>,
            std::array<bit_word_t<Bits>, bit_word_count<Bits>>
        >;

        template <typename Word>
        class BitReference;
    }

    /**
     * Fixed-size packed bit array.
     *
     * Storage uses the smallest fixed-width integer that can hold the requested
     * bits without exceeding the platform's native word size. Larger arrays are
     * split across native words.
     *
     * Bit zero is the least-significant bit. Mutable indexing returns a proxy so
     * individual bits can be assigned as though they were ordinary booleans.
     */
    template <std::size_t Bits>
    class BitArray {
        static_assert(Bits > 0);

        using word_type = Detail::bit_word_t<Bits>;
        using storage_type = Detail::bit_storage_t<Bits>;

        static constexpr std::size_t word_bits = Detail::bit_word_bits<Bits>;
        static constexpr std::size_t word_count = Detail::bit_word_count<Bits>;

    public:
        using Reference = Detail::BitReference<word_type>;

        Reference operator[](std::size_t index) noexcept;
        bool operator[](std::size_t index) const noexcept;

    private:
        static constexpr word_type mask(std::size_t index) noexcept;

        word_type& word(std::size_t index) noexcept;
        const word_type& word(std::size_t index) const noexcept;

        storage_type storage_{};
    };
}

namespace CE::math{
    namespace Detail {
        /**
         * Writable proxy for one bit inside a BitArray backing word.
         *
         * Construction identifies the backing word and bit mask. Assignment writes
         * through that reference rather than rebinding it to another bit.
         */
        template <typename Word>
        class BitReference {
        public:
            BitReference& operator=(bool value) noexcept;
            BitReference& operator=(const BitReference& other) noexcept;
            explicit operator bool() const noexcept;

        private:
            template <std::size_t>
            friend class BitArray;

            BitReference(Word& word, Word mask) noexcept;

            Word* word_;
            Word mask_;
        };

        template <typename Word>
        BitReference<Word>::BitReference(Word& word, const Word mask) noexcept
        : word_(&word), mask_(mask) {}

        template <typename Word>
        BitReference<Word>& BitReference<Word>::operator=(const bool value) noexcept {
            if (value) {
                *word_ |= mask_;
            }
            else {
                *word_ &= static_cast<Word>(~mask_);
            }
            return *this;
        }

        template <typename Word>
        BitReference<Word>& BitReference<Word>::operator=(const BitReference& other) noexcept {
            if (this != &other) {
                *this = static_cast<bool>(other);
            }
            return *this;
        }

        template <typename Word>
        BitReference<Word>::operator bool() const noexcept {
            return (*word_ & mask_) != 0;
        }
    }

    template <std::size_t Bits>
    typename BitArray<Bits>::Reference BitArray<Bits>::operator[](const std::size_t index) noexcept {
        assert(index < Bits);
        return {word(index), mask(index)};
    }

    template <std::size_t Bits>
    bool BitArray<Bits>::operator[](const std::size_t index) const noexcept {
        assert(index < Bits);
        return (word(index) & mask(index)) != 0;
    }

    template <std::size_t Bits>
    constexpr typename BitArray<Bits>::word_type BitArray<Bits>::mask(const std::size_t index) noexcept {
        return word_type{1} << (index % word_bits);
    }

    template <std::size_t Bits>
    typename BitArray<Bits>::word_type& BitArray<Bits>::word([[maybe_unused]] const std::size_t index) noexcept {
        if constexpr (word_count == 1) {
            return storage_;
        }
        else {
            return storage_[index / word_bits];
        }
    }

    template <std::size_t Bits>
    const typename BitArray<Bits>::word_type& BitArray<Bits>::word(
        [[maybe_unused]] const std::size_t index
    ) const noexcept {
        if constexpr (word_count == 1) {
            return storage_;
        }
        else {
            return storage_[index / word_bits];
        }
    }

    template <std::size_t Bits>
    std::ostream& operator<<(std::ostream& out, const BitArray<Bits>& bits) {
        for (std::size_t i = Bits; i > 0; --i) {
            out << (bits[i - 1] ? '1' : '0');
        }
        return out;
    }
}
