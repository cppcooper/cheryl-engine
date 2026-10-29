#include <core/game-framework/frame-pacer.h>
#include <internals/exceptions.h>
#include <gtest/gtest.h>

#include <chrono>
#include <limits>

using CE::Runtime::FramePacer;
using CE::Runtime::FramePacingOptions;
using Clock = FramePacer::Clock;
using namespace std::chrono_literals;

TEST(frame_pacing, defaults) {
    FramePacer pacer;
    EXPECT_FALSE(pacer.options().enabled);
    EXPECT_DOUBLE_EQ(pacer.options().target_fps, 60);
    const auto start = Clock::time_point{};
    EXPECT_TRUE(pacer.due(start));
    pacer.frame_started(start);
    EXPECT_TRUE(pacer.due(start));
    EXPECT_EQ(pacer.next_frame_at(), Clock::time_point::max());
}

TEST(frame_pacing, target) {
    FramePacer pacer({.enabled = true});
    const auto start = Clock::time_point{1s};
    EXPECT_TRUE(pacer.due(start));
    EXPECT_EQ(pacer.next_frame_at(), Clock::time_point::max());
    pacer.frame_started(start);
    const auto interval = std::chrono::ceil<Clock::duration>(std::chrono::duration<long double>{1.0L / 60});
    EXPECT_EQ(pacer.next_frame_at(), start + interval);
    EXPECT_FALSE(pacer.due(start + interval - Clock::duration{1}));
    EXPECT_TRUE(pacer.due(start + interval));
}

TEST(frame_pacing, render_work) {
    FramePacer pacer({.enabled = true, .target_fps = 50});
    const auto start = Clock::time_point{1s};
    pacer.frame_started(start);
    // Work and vsync time count toward the 20 ms interval, rather than adding
    // another complete interval after presentation returns.
    EXPECT_FALSE(pacer.due(start + 15ms));
    EXPECT_EQ(pacer.next_frame_at(), start + 20ms);
    EXPECT_TRUE(pacer.due(start + 25ms));
}

TEST(frame_pacing, no_catchup) {
    FramePacer pacer({.enabled = true, .target_fps = 50});
    const auto start = Clock::time_point{1s};
    pacer.frame_started(start);
    const auto late = start + 1s;
    EXPECT_TRUE(pacer.due(late));
    pacer.frame_started(late);
    EXPECT_FALSE(pacer.due(late));
    EXPECT_EQ(pacer.next_frame_at(), late + 20ms);
}

TEST(frame_pacing, fractional) {
    FramePacer pacer({.enabled = true, .target_fps = 59.94});
    const auto start = Clock::time_point{};
    pacer.frame_started(start);
    const auto elapsed = std::chrono::duration<long double>(pacer.next_frame_at() - start).count();
    EXPECT_GE(elapsed, 1.0L / static_cast<long double>(59.94));
    EXPECT_LT(elapsed - 1.0L / static_cast<long double>(59.94), std::chrono::duration<long double>(Clock::duration{1}).count());
}

TEST(frame_pacing, invalid) {
    for (const auto target :
         {0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(),
          std::numeric_limits<double>::denorm_min()}) {
        for (const bool enabled : {false, true}) {
            SCOPED_TRACE(target);
            EXPECT_THROW(static_cast<void>(FramePacer(FramePacingOptions{enabled, target})), CE::Exceptions::invalid_args);
        }
    }
}

TEST(frame_pacing, clock_limits) {
    FramePacer fast({.enabled = true, .target_fps = std::numeric_limits<double>::max()});
    const auto start = Clock::time_point{};
    fast.frame_started(start);
    EXPECT_EQ(fast.next_frame_at(), start + Clock::duration{1});
    FramePacer slow({.enabled = true});
    slow.frame_started(Clock::time_point::max() - Clock::duration{1});
    EXPECT_EQ(slow.next_frame_at(), Clock::time_point::max());
}
