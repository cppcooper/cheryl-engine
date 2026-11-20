#pragma once

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string_view>

namespace CE::Story {
    struct TimelineId {
        std::uint64_t value{};
        auto operator<=>(const TimelineId&) const = default;
    };

    template<class Tag> struct Ref {
        TimelineId timeline{};
        std::uint64_t value{};
        auto operator<=>(const Ref&) const = default;
        explicit operator bool() const { return timeline.value != 0 && value != 0; }
    };
    using PersonId = Ref<struct PersonTag>;
    using ObjectId = Ref<struct ObjectTag>;
    using CommitmentId = Ref<struct CommitmentTag>;
    using EpisodeId = Ref<struct EpisodeTag>;

    struct Origin {
        TimelineId timeline{};
        std::uint64_t identity{};
        auto operator<=>(const Origin&) const = default;
    };
    struct GenerationAddress {
        std::uint64_t family{};
        std::uint64_t creation{};
        auto operator<=>(const GenerationAddress&) const = default;
    };
    struct StoryTime {
        std::int64_t value{};
        auto operator<=>(const StoryTime&) const = default;
        static std::optional<StoryTime> add(StoryTime time, std::int64_t delta) {
            if ((delta > 0 && time.value > std::numeric_limits<std::int64_t>::max() - delta) ||
                (delta < 0 && time.value < std::numeric_limits<std::int64_t>::min() - delta)) {
                return std::nullopt;
            }
            return StoryTime{time.value + delta};
        }
    };
    struct EventOrder {
        StoryTime time{};
        std::uint32_t phase{};
        std::uint64_t address{};
        auto operator<=>(const EventOrder&) const = default;
    };
    struct Text {
        std::array<char, 96> bytes{};
        std::uint8_t size{};
        auto operator<=>(const Text&) const = default;
        static std::optional<Text> make(std::string_view input) {
            if (input.size() > 96) {
                return std::nullopt;
            }
            Text result;
            result.size = static_cast<std::uint8_t>(input.size());
            for (std::size_t index = 0; index < input.size(); ++index) {
                result.bytes[index] = input[index];
            }
            return result;
        }
        std::string_view view() const { return {bytes.data(), size}; }
    };
    enum class Error {
        None, InvalidSeed, ForeignReference, Missing, Duplicate, Cycle, Life, Depth, Time,
        Authority, Stale, Conflict, Expired, Capacity, Bytes, WorkLimit, Consent, Terms,
        NotHolder, Terminal, UnknownAccount, NotContact, Ineligible, Unsupported
    };
    struct Limits {
        std::size_t people{64};
        std::size_t objects{32};
        std::size_t commitments{128};
        std::size_t episodes{512};
        std::size_t acquisitions{1024};
        std::size_t links{256};
        std::size_t receipts{1024};
        std::size_t bytes{4 * 1024 * 1024};
        std::size_t queue{128};
        std::size_t work_per_action{64};
        std::size_t output_bytes{64 * 1024};
        auto operator<=>(const Limits&) const = default;
    };
    struct Counters {
        std::uint64_t records_read{};
        std::uint64_t records_touched{};
        std::uint64_t links_traversed{};
        std::uint64_t preparations{};
        std::uint64_t retries{};
        std::uint64_t validation_passes{};
        std::uint64_t whole_state_copies{};
    };
}
