#pragma once
#include <chrono>

#pragma once

#include <chrono>

template <typename ClockType = std::chrono::steady_clock>
class DeltaTime {
public:
    using TimePoint = typename ClockType::time_point;
    using Duration = typename ClockType::duration;

private:
    TimePoint last_checkin;

protected:
    [[nodiscard]] Duration elapsed(const TimePoint now) const {
        return now - last_checkin;
    }

    void checkin(const TimePoint now) {
        last_checkin = now;
    }

public:
    /** Starts the first timing interval at construction. */
    DeltaTime() : last_checkin(ClockType::now()) {}

    /** Returns the elapsed interval in seconds and begins the next interval at the same sampled time. */
    double operator()() {
        const auto now = ClockType::now();
        const double delta = std::chrono::duration<double>(elapsed(now)).count();
        checkin(now);
        return delta;
    }

    /** Returns the duration since the most recent check-in without beginning a new interval. */
    [[nodiscard]] Duration elapsed() const {
        return elapsed(ClockType::now());
    }

    /** Ends the current interval and begins a new one at the current time. */
    void checkin() {
        checkin(ClockType::now());
    }
};
