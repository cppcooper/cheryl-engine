#pragma once
#include <chrono>


#include <chrono>

namespace CE::Time {

    /** Caller-owned interval timer, without synchronization.
     * @tparam ClockType Clock providing now(), time_point and duration; defaults to steady_clock.
     * The clock determines tick representation and backward-jump behavior. No fixed
     * timestep, smoothing, clamping or scheduling policy is added.
     * @see [Simulation timing](cheryl-guide:runtime/simulation-timing)
     */
    template <typename ClockType = std::chrono::steady_clock> class DeltaTime {
    public:
        /** Time point in the selected clock's domain; not interchangeable across clocks. */
        using TimePoint = typename ClockType::time_point;
        /** Native duration of the selected clock, before conversion to seconds. */
        using Duration = typename ClockType::duration;

    private:
        TimePoint last_checkin;

    protected:
        [[nodiscard]] Duration elapsed(const TimePoint now) const { return now - last_checkin; }

        void checkin(const TimePoint now) { last_checkin = now; }

    public:
        DeltaTime()
        : last_checkin(ClockType::now()) {}

        double operator()() {
            const auto now = ClockType::now();
            const double delta = std::chrono::duration<double>(elapsed(now)).count();
            checkin(now);
            return delta;
        }

        [[nodiscard]] Duration elapsed() const { return elapsed(ClockType::now()); }

        void checkin() { checkin(ClockType::now()); }
    };

}

/** @fn template<typename ClockType> CE::Time::DeltaTime<ClockType>::DeltaTime()
 * @brief Begins the first interval at one ClockType::now() sample.
 * Clock failures, if any, propagate; construction does not sleep or start a thread.
 */

/** @fn template<typename ClockType> double CE::Time::DeltaTime<ClockType>::operator()()
 * @brief Returns elapsed seconds and begins the next interval at the same sampled instant.
 * @return Floating seconds since the previous check-in; a custom clock may yield negative values.
 * Samples the clock once; repeated calls measure successive intervals. No synchronization is added.
 */

/** @fn template<typename ClockType> typename CE::Time::DeltaTime<ClockType>::Duration CE::Time::DeltaTime<ClockType>::elapsed() const
 * @brief Reads the native duration without starting a new interval.
 * @return Clock duration since the previous check-in, not necessarily whole seconds.
 * The next read still measures from the same check-in.
 */

/** @fn template<typename ClockType> typename CE::Time::DeltaTime<ClockType>::Duration CE::Time::DeltaTime<ClockType>::elapsed(TimePoint now) const
 * @brief Computes a duration using a caller-supplied time point.
 * @param now Sample in ClockType's time domain; no clock read or state update occurs.
 * @return now minus the previous check-in; no range/backward-jump validation.
 */

/** @fn template<typename ClockType> void CE::Time::DeltaTime<ClockType>::checkin()
 * @brief Starts a new interval at the current clock sample.
 * Discards the unread elapsed interval; clock failures propagate before assignment.
 */

/** @fn template<typename ClockType> void CE::Time::DeltaTime<ClockType>::checkin(TimePoint now)
 * @brief Starts a new interval at the supplied time point.
 * @param now New interval origin in ClockType's domain; no clock read occurs.
 */
