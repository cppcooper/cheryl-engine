#pragma once
#include <chrono>

template<typename ClockType = std::chrono::steady_clock>
class DeltaTime {
public:
    using TimePoint = typename ClockType::time_point;
    using Duration  = typename ClockType::duration; // ns by default

protected:
    // TODO: Start the timer at runtime initialization (or define the first sample as zero).
    // A default-constructed time_point is the clock epoch, so the first operator() reports
    // time since that epoch instead of the interval since the game loop began.
    TimePoint last_checkin;

    void checkin() { last_checkin = ClockType::now(); }
    Duration elapsed() const { return ClockType::now() - last_checkin; }

public:
    double operator()() {
        const double delta = std::chrono::duration<double>(elapsed()).count();
        checkin();
        return delta;
    }
};
