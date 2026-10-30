#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ostream>
#include <span>
#include <type_traits>

namespace CE::Math {
    namespace Detail {
        // Use size_t as Cheryl's native storage word. Small bit arrays stay in the
        // narrowest fixed-width integer that can contain them; once Bits exceeds
        // the native word width, storage is split across native-word elements.
        using native_word = std::size_t;
        inline constexpr std::size_t native_bits = std::numeric_limits<native_word>::digits;

        // Select the smallest fixed-width integer that can hold Bits without
        // exceeding the native word size. If none qualify, use the native word.
        template <std::size_t Bits>
        using bit_word_t = std::conditional_t<
            Bits <= 8,
            std::uint8_t,
            std::conditional_t<
                Bits <= 16 && native_bits >= 16,
                std::uint16_t,
                std::conditional_t<
                    Bits <= 32 && native_bits >= 32,
                    std::uint32_t,
                    std::conditional_t<Bits <= 64 && native_bits >= 64, std::uint64_t, native_word>>>>;

        // Width, in bits, of the selected backing word.
        template <std::size_t Bits> inline constexpr std::size_t bit_word_bits = std::numeric_limits<bit_word_t<Bits>>::digits;

        // Number of backing words required to store Bits.
        template <std::size_t Bits> inline constexpr std::size_t bit_word_count = (Bits + bit_word_bits<Bits> - 1) / bit_word_bits<Bits>;

        // Keep single-word bit arrays as a scalar; larger arrays become a packed
        // sequence of backing words.
        template <std::size_t Bits>
        using bit_storage_t =
            std::conditional_t<bit_word_count<Bits> == 1, bit_word_t<Bits>, std::array<bit_word_t<Bits>, bit_word_count<Bits>>>;

        // Definition below
        template <typename Word> class BitReference;
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
     * words() exposes the same packed storage as a contiguous word array.
     * Indices must be below Bits; assertions are not checked release-mode errors.
     * Spans/proxies borrow this array. Access is unsynchronized, including writes
     * to different bits in one word; whole-word access can modify unused high bits.
     * Native storage/byte order is not a portable serialized representation.
     * @tparam Bits Positive logical bit count, enforced by static_assert; starts with all bits zero.
     * @see [Utility contracts](cheryl-guide:runtime/utility-contracts)
     */
    template <std::size_t Bits> class BitArray {
        static_assert(Bits > 0);

    public:
        /** Smallest supported backing word not wider than native size_t. */
        using word_type = ::CE::Math::Detail::bit_word_t<Bits>;
        /** Borrowed write-through bit proxy; see mutable operator[] for assignment and lifetime rules. */
        using Reference = ::CE::Math::Detail::BitReference<word_type>;

        static constexpr std::size_t word_bits = ::CE::Math::Detail::bit_word_bits<Bits>; ///< Bits per backing word.
        static constexpr std::size_t word_count = ::CE::Math::Detail::bit_word_count<Bits>; ///< Words required for Bits, rounding up.

        Reference operator[](std::size_t index) noexcept;
        bool operator[](std::size_t index) const noexcept;

        auto words() noexcept -> std::span<word_type, word_count>;
        auto words() const noexcept -> std::span<const word_type, word_count>;

    private:
        using storage_type = ::CE::Math::Detail::bit_storage_t<Bits>;

        static constexpr word_type mask(std::size_t index) noexcept;

        word_type& word(std::size_t index) noexcept;
        const word_type& word(std::size_t index) const noexcept;

        storage_type storage_{};
    };
}

namespace CE::Math {
    namespace Detail {
        /**
         * Writable proxy for one bit inside a BitArray backing word.
         *
         * Construction identifies the backing word and bit mask. Assignment writes
         * through that reference rather than rebinding it to another bit.
         */
        template <typename Word> class BitReference {
        public:
            BitReference& operator=(bool value) noexcept;
            BitReference& operator=(const BitReference& other) noexcept;
            explicit operator bool() const noexcept;

        private:
            template <std::size_t> friend class ::CE::Math::BitArray;

            BitReference(Word& word, Word mask) noexcept;

            Word* word_;
            Word mask_;
        };

        template <typename Word>
        BitReference<Word>::BitReference(Word& word, const Word mask) noexcept
        : word_(&word), mask_(mask) {}

        template <typename Word> BitReference<Word>& BitReference<Word>::operator=(const bool value) noexcept {
            if (value) {
                *word_ |= mask_;
            } else {
                *word_ &= static_cast<Word>(~mask_);
            }
            return *this;
        }

        template <typename Word> BitReference<Word>& BitReference<Word>::operator=(const BitReference& other) noexcept {
            if (this != &other) {
                *this = static_cast<bool>(other);
            }
            return *this;
        }

        template <typename Word> BitReference<Word>::operator bool() const noexcept {
            return (*word_ & mask_) != 0;
        }
    }

    /** Borrows a writable bit proxy; indexing performs no allocation.
     * @param index Zero-based bit index, strictly below Bits; asserted, not a checked release error.
     * @return Proxy borrowing this array's backing word. Assignment from bool writes that
     * bit; assignment from another proxy copies its current bit value, not its binding.
     * Explicit bool conversion samples the bit. Keep this array alive through proxy use;
     * copied/moved arrays have separate storage and do not retarget existing proxies.
     * Writes, including different bits sharing a word, require caller synchronization.
     * @see [Packed bit arrays](cheryl-guide:runtime/utility-contracts)
     */
    template <std::size_t Bits> typename BitArray<Bits>::Reference BitArray<Bits>::operator[](const std::size_t index) noexcept {
        assert(index < Bits);
        return {word(index), mask(index)};
    }

    /** Reads one bit as an independent bool value.
     * @param index Zero-based bit index below Bits; violations are assertion-only.
     * @return Current bit value; no pointer/proxy or synchronization is retained.
     */
    template <std::size_t Bits> bool BitArray<Bits>::operator[](const std::size_t index) const noexcept {
        assert(index < Bits);
        return (word(index) & mask(index)) != 0;
    }

    /** Borrows contiguous mutable backing words in least-significant-bit index order.
     * @return Fixed-extent word span borrowing this array. It does not mask unused high bits.
     * Caller mutations and synchronization operate on whole words; this is native storage,
     * not a portable byte serialization. Keep the owning array alive through use.
     */
    template <std::size_t Bits> auto BitArray<Bits>::words() noexcept -> std::span<word_type, word_count> {
        return std::span<word_type, word_count>{&word(0), word_count};
    }

    /** Borrows a read-only span of contiguous native backing words.
     * @return Fixed-extent view valid while this array lives, including any unused high bits.
     * It is a live view, not a snapshot; reads must not race mutation of the same words.
     */
    template <std::size_t Bits> auto BitArray<Bits>::words() const noexcept -> std::span<const word_type, word_count> {
        return std::span<const word_type, word_count>{&word(0), word_count};
    }

    template <std::size_t Bits> constexpr typename BitArray<Bits>::word_type BitArray<Bits>::mask(const std::size_t index) noexcept {
        return word_type{1} << (index % word_bits);
    }

    template <std::size_t Bits>
    typename BitArray<Bits>::word_type& BitArray<Bits>::word([[maybe_unused]] const std::size_t index) noexcept {
        if constexpr (word_count == 1) {
            return storage_;
        } else {
            return storage_[index / word_bits];
        }
    }

    template <std::size_t Bits>
    const typename BitArray<Bits>::word_type& BitArray<Bits>::word([[maybe_unused]] const std::size_t index) const noexcept {
        if constexpr (word_count == 1) {
            return storage_;
        } else {
            return storage_[index / word_bits];
        }
    }

    /** Writes Bits binary digits, highest bit first and without separators or a prefix.
     * @tparam Bits Positive logical width; unused high storage bits are omitted.
     * @param out Borrowed destination stream; its state/error policy controls failures.
     * @param bits Borrowed packed array, read synchronously without locking.
     * @return The same stream for chaining. Stream errors/exception masks are preserved.
     */
    template <std::size_t Bits> std::ostream& operator<<(std::ostream& out, const BitArray<Bits>& bits) {
        for (std::size_t i = Bits; i > 0; --i) {
            out << (bits[i - 1] ? '1' : '0');
        }
        return out;
    }
}
