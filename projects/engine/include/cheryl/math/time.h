#pragma once
#include <chrono>
#include <type_traits>

namespace CE::Time {

    // Signed nanoseconds using the platform's long representation; aliases carry units,
    // not timestamps or clock ownership.
    using ElapsedDuration = std::chrono::duration<signed long int, std::ratio<1, 1000000000>>;
    using Hours = std::chrono::hours;
    using Minutes = std::chrono::minutes;
    using Seconds = std::chrono::seconds;
    using Milliseconds = std::chrono::milliseconds;

    template <typename T> struct is_chrono_duration : std::false_type {};

    template <typename Rep, typename Period> struct is_chrono_duration<std::chrono::duration<Rep, Period>> : std::true_type {};

    template <typename T> constexpr bool is_chrono_duration_v = is_chrono_duration<T>::value;

    // Standard duration_cast semantics (integer results truncate toward zero). Callers
    // ensure conversion fits Out; no overflow or floating-nonfinite validation is added.
    template <typename In, typename Out> Out duration_cast(const In& t) {
        static_assert(is_chrono_duration_v<Out>, "tcast can only cast durations to other durations");
        static_assert(is_chrono_duration_v<In>, "tcast can only cast durations to other durations");
        return std::chrono::duration_cast<Out>(t);
    }

    template <typename In> Hours to_hours(const In& t) {
        return duration_cast<In, Hours>(t);
    }

    template <typename In> Minutes to_minutes(const In& t) {
        return duration_cast<In, Minutes>(t);
    }

    template <typename In> Seconds to_seconds(const In& t) {
        return duration_cast<In, Seconds>(t);
    }

    template <typename In> Milliseconds to_milliseconds(const In& t) {
        return duration_cast<In, Milliseconds>(t);
    }

}
