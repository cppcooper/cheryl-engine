#include <backends/miniaudio.h>
#include <internals/exceptions.h>
#include "system-internal.h"
#include "wav.h"
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

namespace {
    using CE::Audio::PlaybackEndReason;
    using CE::Audio::PlaybackState;
    using CE::Audio::Miniaudio::System;
    using CE::Audio::Miniaudio::Detail::SystemAccess;
    using namespace std::chrono_literals;

    auto clip(std::size_t frames = 512) {
        return CE::Audio::Clip::from_samples({1, 8000}, std::vector<float>(frames, 0.25f));
    }

    void expect_samples(std::span<const float> samples, float value) {
        for (const float sample : samples)
            EXPECT_NEAR(sample, value, 0.00001f);
    }
}

TEST(audio_schedule, boundary) {
    auto output = System::open_offline({1, 8000});
    auto voice = output->play(clip(), {.paused = true});
    voice->resume_after(10ms);
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Scheduled);
    output->maintain();
    output->publish_notifications();
    EXPECT_EQ(output->notification_snapshot().sequence, 0);
    std::array<float, 64> samples{};
    output->render(samples);
    expect_samples(samples, 0);
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Scheduled);
    output->render(samples);
    expect_samples(std::span(samples).first(16), 0);
    expect_samples(std::span(samples).subspan(16), 0.25f);
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Playing);
    EXPECT_EQ(voice->snapshot().generation, 1);
    EXPECT_FALSE(voice->snapshot().terminal);
}

TEST(audio_schedule, quantization) {
    auto output = System::open_offline({1, 8000});
    auto voice = output->play(clip(), {.paused = true});
    voice->resume_after(std::chrono::duration<double>{1.25 / 8000});
    std::array<float, 8> samples{};
    output->render(samples);
    expect_samples(std::span(samples).first(2), 0);
    expect_samples(std::span(samples).subspan(2), 0.25f);
    voice->pause();
    voice->resume_after(0ms);
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Playing);
    output->render(samples);
    expect_samples(samples, 0.25f);
}

TEST(audio_schedule, pause) {
    auto output = System::open_offline({1, 8000});
    auto voice = output->play(clip(), {.paused = true});
    voice->resume_after(100ms);
    std::array<float, 32> samples{};
    output->render(samples);
    voice->pause();
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Paused);
    output->render(samples);
    expect_samples(samples, 0);
    // Resume before the old deadline: cancellation must remove its native gate.
    voice->resume();
    output->render(samples);
    expect_samples(samples, 0.25f);
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Playing);
}

TEST(audio_schedule, cursor) {
    auto output = System::open_offline({1, 8000});
    std::vector<float> source(512, 0.5f);
    std::fill_n(source.begin(), 32, 0.125f);
    auto voice = output->play(CE::Audio::Clip::from_samples({1, 8000}, std::move(source)));
    std::array<float, 32> samples{};
    output->render(samples);
    expect_samples(samples, 0.125f);
    voice->pause();
    voice->resume_after(4ms);
    output->render(samples);
    expect_samples(samples, 0);
    output->render(samples);
    expect_samples(samples, 0.5f);
}

TEST(audio_schedule, stop_restart) {
    auto output = System::open_offline({1, 8000});
    auto voice = output->play(clip(), {.paused = true});
    voice->resume_after(100ms);
    voice->stop();
    voice->resume_after(1ms);
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Stopped);
    ASSERT_TRUE(voice->snapshot().terminal);
    EXPECT_EQ(voice->snapshot().terminal->reason, PlaybackEndReason::Stopped);
    std::array<float, 64> samples{};
    output->render(samples);
    expect_samples(samples, 0);
    voice->restart();
    output->render(samples);
    expect_samples(samples, 0.25f);
    voice->pause();
    voice->resume_after(100ms);
    voice->restart();
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Playing);
    EXPECT_EQ(voice->snapshot().generation, 3);
    output->render(samples);
    expect_samples(samples, 0.25f);
}

TEST(audio_schedule, retained) {
    auto output = System::open_offline({1, 8000});
    auto voice = output->play(clip(16), {.paused = true});
    voice->resume_after(10ms);
    voice.reset();
    output->maintain();
    EXPECT_EQ(output->notification_snapshot().sequence, 0);
    std::array<float, 128> samples{};
    output->render(samples);
    expect_samples(std::span(samples).first(80), 0);
    expect_samples(std::span(samples).subspan(80, 16), 0.25f);
    expect_samples(std::span(samples).subspan(96), 0);
    output->maintain();
    EXPECT_EQ(output->notification_snapshot().sequence, 1);
}

TEST(audio_schedule, stream) {
    AudioFixture::Wav fixture;
    fixture.write({1, 8000}, std::vector<float>(512, 0.25f));
    auto output = System::open_offline({1, 8000});
    auto voice = output->stream(fixture.file(), {.paused = true});
    voice->resume_after(10ms);
    std::array<float, 128> samples{};
    output->render(samples);
    expect_samples(std::span(samples).first(80), 0);
    expect_samples(std::span(samples).subspan(80), 0.25f);
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Playing);
}

TEST(audio_schedule, rate) {
    auto output = System::open_offline({1, 8000});
    auto voice = output->play(clip(8192), {.paused = true, .variable_rate = true});
    voice->resume_after(100ms);
    voice->set_playback_rate(2);
    std::array<float, 400> samples{};
    output->render(samples);
    expect_samples(samples, 0);
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Scheduled);
    output->render(samples);
    expect_samples(samples, 0);
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Playing);
    output->render(samples);
    expect_samples(std::span(samples).subspan(128), 0.25f);
    voice->pause();
    voice->resume_after(1s);
    voice->restart();
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Playing);
    EXPECT_FLOAT_EQ(voice->snapshot().playback_rate, 2);
}

TEST(audio_schedule, failures) {
    auto output = System::open_offline({1, 8000});
    auto voice = output->play(clip(), {.paused = true});
    SystemAccess::fail_next_start(*output);
    EXPECT_THROW(voice->resume_after(10ms), CE::Exceptions::runtime_exception);
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Paused);
    EXPECT_FALSE(voice->snapshot().terminal);
    voice->resume();
    std::array<float, 32> samples{};
    output->render(samples);
    expect_samples(samples, 0.25f);
    voice->pause();
    voice->resume_after(4ms);
    SystemAccess::fail_next_stop(*output);
    EXPECT_THROW(voice->pause(), CE::Exceptions::runtime_exception);
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Scheduled);
    output->render(samples);
    expect_samples(samples, 0);
    output->render(samples);
    expect_samples(samples, 0.25f);
    EXPECT_EQ(output->notification_snapshot().sequence, 0);
}

TEST(audio_schedule, invalid) {
    auto output = System::open_offline({1, 8000});
    auto voice = output->play(clip(), {.paused = true});
    for (const double seconds : {-1.0, std::numeric_limits<double>::quiet_NaN(),
             std::numeric_limits<double>::infinity(), std::numeric_limits<double>::max(),
             std::ldexp(1.0, 64) / 8000}) {
        EXPECT_THROW(voice->resume_after(std::chrono::duration<double>{seconds}), CE::Exceptions::invalid_args);
        EXPECT_EQ(voice->snapshot().state, PlaybackState::Paused);
    }
    voice->resume();
    std::array<float, 32> samples{};
    output->render(samples);
    expect_samples(samples, 0.25f);
}

TEST(audio_schedule, notifications) {
    auto output = System::open_offline({1, 8000});
    CE::Events::EventBus events;
    std::vector<CE::Audio::PlaybackEnded> ended;
    const auto listener = events.register_listener(CE::Audio::playback_ended_event,
        [&](const CE::Audio::PlaybackEnded& event) { ended.push_back(event); });
    output->publish_events_to(events.publisher());
    auto voice = output->play(clip(8), {.paused = true});
    voice->resume_after(10ms);
    output->publish_notifications();
    EXPECT_TRUE(ended.empty());
    std::array<float, 128> samples{};
    output->render(samples);
    output->publish_notifications();
    ASSERT_EQ(ended.size(), 1);
    EXPECT_EQ(ended[0].terminal.reason, PlaybackEndReason::Completed);
    voice->resume_after(1ms);
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Finished);
    voice->restart();
    voice->pause();
    voice->resume_after(1s);
    output->close();
    output->publish_notifications();
    ASSERT_EQ(ended.size(), 2);
    EXPECT_EQ(ended[1].terminal.reason, PlaybackEndReason::OutputClosed);
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Closed);
    EXPECT_THROW(voice->resume_after(0ms), CE::Exceptions::bad_request);
    listener.disconnect();
}
