#pragma once
#include <chrono>
#include <type_traits>

namespace CE::Time {

    /** Signed nanoseconds using the platform's long representation, not a timestamp.
     * No clock or epoch is attached; representable range follows signed long.
     * @see [Utility contracts](cheryl-guide:runtime/utility-contracts)
     */
    using ElapsedDuration = std::chrono::duration<signed long int, std::ratio<1, 1000000000>>;
    /** Standard integral-hour duration; conversions truncate fractional hours. */
    using Hours = std::chrono::hours;
    /** Standard integral-minute duration; conversions truncate fractional minutes. */
    using Minutes = std::chrono::minutes;
    /** Standard integral-second duration; conversions truncate fractional seconds. */
    using Seconds = std::chrono::seconds;
    /** Standard integral-millisecond duration; conversions truncate fractional milliseconds. */
    using Milliseconds = std::chrono::milliseconds;

    /** False unless T is an exact std::chrono::duration specialization.
     * References and cv-qualified wrappers are not removed before matching.
     * @tparam T Type to inspect without constructing it.
     */
    template <typename T> struct is_chrono_duration : std::false_type {};

    /** Recognizes duration<Rep, Period>; its inherited value is true.
     * @tparam Rep Duration's arithmetic representation.
     * @tparam Period Ratio describing the duration's tick in seconds.
     */
    template <typename Rep, typename Period> struct is_chrono_duration<std::chrono::duration<Rep, Period>> : std::true_type {};

    /** Compile-time value of is_chrono_duration<T>, without cv/ref normalization. */
    template <typename T> constexpr bool is_chrono_duration_v = is_chrono_duration<T>::value;

    /** Converts a duration with standard chrono arithmetic; integer results truncate toward zero.
     * @tparam In Input duration type, checked with a compile-time assertion.
     * @tparam Out Output duration type, checked with a compile-time assertion.
     * @param t Borrowed input duration; no clock/epoch conversion is performed.
     * @return Owned duration in Out's units.
     * @pre The conversion is representable; no overflow or nonfinite-value validation is added.
     */
    template <typename In, typename Out> Out duration_cast(const In& t) {
        static_assert(is_chrono_duration_v<Out>, "tcast can only cast durations to other durations");
        static_assert(is_chrono_duration_v<In>, "tcast can only cast durations to other durations");
        return std::chrono::duration_cast<Out>(t);
    }

    /** Converts to Hours with duration_cast's representation and truncation rules.
     * @tparam In A chrono duration; not a clock time point.
     * @param t Borrowed elapsed interval.
     * @return Owned duration in Hours's units; callers ensure representability.
     */
    template <typename In> Hours to_hours(const In& t) {
        return duration_cast<In, Hours>(t);
    }

    /** Converts to Minutes with duration_cast's representation and truncation rules.
     * @tparam In A chrono duration; not a clock time point.
     * @param t Borrowed elapsed interval.
     * @return Owned duration in Minutes's units; callers ensure representability.
     */
    template <typename In> Minutes to_minutes(const In& t) {
        return duration_cast<In, Minutes>(t);
    }

    /** Converts to Seconds with duration_cast's representation and truncation rules.
     * @tparam In A chrono duration; not a clock time point.
     * @param t Borrowed elapsed interval.
     * @return Owned duration in Seconds's units; callers ensure representability.
     */
    template <typename In> Seconds to_seconds(const In& t) {
        return duration_cast<In, Seconds>(t);
    }

    /** Converts to Milliseconds with duration_cast's representation and truncation rules.
     * @tparam In A chrono duration; not a clock time point.
     * @param t Borrowed elapsed interval.
     * @return Owned duration in Milliseconds's units; callers ensure representability.
     */
    template <typename In> Milliseconds to_milliseconds(const In& t) {
        return duration_cast<In, Milliseconds>(t);
    }

}
