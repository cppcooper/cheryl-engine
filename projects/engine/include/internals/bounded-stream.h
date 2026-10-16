#pragma once
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <streambuf>
#include <string_view>

namespace CE::Diagnostics {
    /** Fixed storage for diagnostic streams. Excess output is consumed/dropped,
     * preserving stream usability; view() marks truncation and never scans beyond
     * the written prefix. reset() allows another use without stale stream state.
     */
    template <std::size_t Capacity> class BoundedStreamBuffer final : public std::streambuf {
        static_assert(Capacity > 1 && Capacity <= std::numeric_limits<int>::max());
        std::array<char, Capacity> data_{};
        bool truncated_ = false;

    public:
        BoundedStreamBuffer() { reset(); }
        void reset() noexcept {
            truncated_ = false;
            data_[0] = '\0';
            setp(data_.data(), data_.data() + Capacity - 1);
        }
        [[nodiscard]] std::string_view view() noexcept {
            const auto size = static_cast<std::size_t>(pptr() - pbase());
            if (truncated_) {
                constexpr std::string_view marker = "... [truncated]";
                const auto count = std::min(size, marker.size());
                std::memcpy(data_.data() + size - count, marker.data(), count);
            }
            data_[size] = '\0';
            return {data_.data(), size};
        }

    protected:
        int_type overflow(int_type character) override {
            if (traits_type::eq_int_type(character, traits_type::eof()))
                return traits_type::not_eof(character);
            truncated_ = true;
            return character;
        }
        std::streamsize xsputn(const char* source, std::streamsize size) override {
            if (size <= 0)
                return 0;
            const auto remaining = static_cast<std::streamsize>(epptr() - pptr());
            const auto count = std::min(remaining, size);
            std::memcpy(pptr(), source, static_cast<std::size_t>(count));
            pbump(static_cast<int>(count));
            truncated_ |= count != size;
            return size;
        }
    };
}
