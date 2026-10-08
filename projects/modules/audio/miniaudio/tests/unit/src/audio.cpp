#include <backends/miniaudio.h>
#include <internals/exceptions.h>
#include <gtest/gtest.h>
#include "wav.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <future>
#include <limits>
#include <memory>
#include <numbers>
#include <vector>

namespace {
    using CE::Audio::Clip;
    using CE::Audio::PlaybackOptions;
    using CE::Audio::PlaybackState;
    using CE::Audio::Miniaudio::System;

    Clip constant(float value = 0.25f, std::size_t frames = 1024) {
        return Clip::from_samples({2, 48000}, std::vector<float>(frames * 2, value));
    }

    void expect_samples(std::span<const float> samples, float expected) {
        for (const float sample : samples)
            EXPECT_NEAR(sample, expected, 0.00001f);
    }
}

TEST(audio_mix, offline) {
    auto system = System::open_offline();
    EXPECT_EQ(system->output_format(), (CE::Audio::Format{2, 48000}));
    EXPECT_EQ(system->backend_name(), "offline");
    EXPECT_FALSE(system->closed());
    auto voice = system->play(constant());
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Playing);
    ASSERT_TRUE(voice->snapshot().duration);
    EXPECT_DOUBLE_EQ(voice->snapshot().duration->count(), 1024.0 / 48000);
    std::array<float, 128> output{};
    system->render(output);
    expect_samples(output, 0.25f);
}

TEST(audio_mix, gain) {
    auto system = System::open_offline();
    const auto clip = constant();
    auto first = system->play(clip);
    auto second = system->play(clip, {.volume = 0.5f});
    system->set_volume(0.5f);
    EXPECT_FLOAT_EQ(system->volume(), 0.5f);
    std::array<float, 128> output{};
    system->render(output);
    expect_samples(output, 0.1875f);
    first->set_volume(0);
    second->set_volume(0);
    system->render(output);
    expect_samples(output, 0);
    EXPECT_FLOAT_EQ(first->snapshot().volume, 0);
}

TEST(audio_mix, cursors) {
    auto system = System::open_offline();
    std::vector<float> samples(2048, 0.5f);
    std::fill_n(samples.begin(), 64, 0.125f);
    const auto clip = Clip::from_samples({2, 48000}, samples);
    auto first = system->play(clip);
    std::array<float, 64> output{};
    system->render(output);
    expect_samples(output, 0.125f);
    first->pause();
    system->render(output);
    expect_samples(output, 0);
    auto second = system->play(clip);
    system->render(output);
    expect_samples(output, 0.125f);
    second->stop();
    first->resume();
    system->render(output);
    expect_samples(output, 0.5f);
    first->restart();
    system->render(output);
    expect_samples(output, 0.125f);
}

TEST(audio_mix, controls) {
    auto system = System::open_offline();
    auto voice = system->play(constant(), {.paused = true});
    std::array<float, 128> output{};
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Paused);
    system->render(output);
    expect_samples(output, 0);
    voice->resume();
    system->render(output);
    expect_samples(output, 0.25f);
    voice->stop();
    voice->resume();
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Stopped);
    voice->set_looping(true);
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Stopped);
    EXPECT_TRUE(voice->snapshot().looping);
    system->render(output);
    expect_samples(output, 0);
    voice->restart();
    system->render(output);
    expect_samples(output, 0.25f);
}

TEST(audio_mix, end_loop) {
    auto system = System::open_offline();
    auto voice = system->play(constant(0.125f, 32), {.looping = true});
    std::array<float, 512> output{};
    system->render(output);
    expect_samples(output, 0.125f);
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Playing);
    voice->set_looping(false);
    system->render(output);
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Finished);
    voice->resume();
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Finished);
    system->render(output);
    expect_samples(output, 0);
    voice->restart();
    system->render(output);
    expect_samples(std::span(output).first(64), 0.125f);
    expect_samples(std::span(output).subspan(64), 0);
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Finished);
}

TEST(audio_mix, retained) {
    auto system = System::open_offline();
    static_cast<void>(system->play(constant(0.125f, 64)));
    system->maintain();
    std::array<float, 64> output{};
    system->render(output);
    expect_samples(output, 0.125f);
    system->render(output);
    expect_samples(output, 0.125f);
    system->render(output);
    expect_samples(output, 0);
    system->maintain();
    system->render(output);
    expect_samples(output, 0);
}

TEST(audio_mix, rates) {
    auto system = System::open_offline();
    const auto clip = Clip::from_samples({1, 24000}, std::vector<float>(2048, 0.25f));
    auto voice = system->play(clip);
    std::array<float, 1024> output{};
    system->render(output);
    expect_samples(std::span(output).subspan(256), 0.25f);
    EXPECT_DOUBLE_EQ(voice->snapshot().duration->count(), 2048.0 / 24000);
    auto other = System::open_offline({1, 8000});
    auto other_voice = other->play(Clip::from_samples({1, 8000}, std::vector<float>(128, 0.5f)));
    std::array<float, 32> mono{};
    other->render(mono);
    expect_samples(mono, 0.5f);
    system->close();
    EXPECT_EQ(other_voice->snapshot().state, PlaybackState::Playing);
}

TEST(audio_mix, closure) {
    std::shared_ptr<CE::Audio::Voice> voice;
    {
        auto system = System::open_offline();
        voice = system->play(constant(), {.looping = true});
        system->close();
        system->close();
        EXPECT_TRUE(system->closed());
        EXPECT_EQ(voice->snapshot().state, PlaybackState::Closed);
        EXPECT_THROW(static_cast<void>(system->play(constant())), CE::Exceptions::bad_request);
        EXPECT_THROW(system->maintain(), CE::Exceptions::bad_request);
        EXPECT_THROW(static_cast<void>(system->volume()), CE::Exceptions::bad_request);
        EXPECT_THROW(system->set_volume(0.5f), CE::Exceptions::bad_request);
        EXPECT_THROW(system->render({}), CE::Exceptions::bad_request);
    }
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Closed);
    EXPECT_THROW(voice->pause(), CE::Exceptions::bad_request);
    EXPECT_THROW(voice->resume(), CE::Exceptions::bad_request);
    EXPECT_THROW(voice->stop(), CE::Exceptions::bad_request);
    EXPECT_THROW(voice->restart(), CE::Exceptions::bad_request);
    EXPECT_THROW(voice->set_volume(0.5f), CE::Exceptions::bad_request);
    EXPECT_THROW(voice->set_looping(false), CE::Exceptions::bad_request);
}

TEST(audio_mix, invalid) {
    EXPECT_THROW(static_cast<void>(System::open_offline({3, 48000})), CE::Exceptions::invalid_args);
    auto system = System::open_offline();
    const auto clip = constant();
    for (const float volume : {-0.5f, 1.5f, std::numeric_limits<float>::quiet_NaN()}) {
        EXPECT_THROW(static_cast<void>(system->play(clip, {volume})), CE::Exceptions::invalid_args);
        EXPECT_THROW(system->set_volume(volume), CE::Exceptions::invalid_args);
    }
    auto voice = system->play(clip);
    EXPECT_THROW(voice->set_volume(-1), CE::Exceptions::invalid_args);
    std::array<float, 1> incomplete{};
    EXPECT_THROW(system->render(incomplete), CE::Exceptions::invalid_args);
    EXPECT_NO_THROW(system->render({}));
}

TEST(audio_mix, producers) {
    auto system = System::open_offline();
    const auto clip = constant();
    std::array<std::future<std::shared_ptr<CE::Audio::Voice>>, 4> producers;
    for (auto& producer : producers)
        producer = std::async(std::launch::async, [&system, clip] {
            auto voice = system->play(clip, {.looping = true});
            for (int index = 0; index < 32; ++index) {
                voice->pause();
                voice->set_volume(0.125f);
                voice->resume();
            }
            return voice;
        });
    for (int index = 0; index < 32; ++index) {
        std::array<float, 128> output{};
        system->render(output);
        system->maintain();
        EXPECT_TRUE(std::ranges::all_of(output, [](float value) { return std::isfinite(value); }));
    }
    std::vector<std::shared_ptr<CE::Audio::Voice>> voices;
    for (auto& producer : producers)
        voices.push_back(producer.get());
    system->close();
    for (const auto& voice : voices)
        EXPECT_EQ(voice->snapshot().state, PlaybackState::Closed);
}

TEST(audio_decode, wav) {
    AudioFixture::Wav fixture;
    const std::array<float, 8> samples{0, 0.25f, -0.5f, 0.75f, 0.125f, -0.25f, 0.5f, -0.75f};
    fixture.write({2, 44100}, samples);
    const auto clip = CE::Audio::Miniaudio::decode_file(fixture.file());
    EXPECT_EQ(clip.format(), (CE::Audio::Format{2, 44100}));
    ASSERT_EQ(clip.samples().size(), samples.size());
    for (std::size_t index = 0; index < samples.size(); ++index)
        EXPECT_NEAR(clip.samples()[index], samples[index], 1.0f / 32768);
    EXPECT_TRUE(std::filesystem::remove(fixture.file()));
    EXPECT_FLOAT_EQ(clip.samples()[1], 0.25f);
}

TEST(audio_decode, limits) {
    AudioFixture::Wav fixture;
    fixture.write({1, 48000}, std::array<float, 128>{});
    EXPECT_THROW(static_cast<void>(CE::Audio::Miniaudio::decode_file(fixture.file(), {127 * sizeof(float)})),
        CE::Exceptions::runtime_exception);
    const auto clip = CE::Audio::Miniaudio::decode_file(fixture.file(), {128 * sizeof(float)});
    EXPECT_EQ(clip.frame_count(), 128);
    EXPECT_THROW(static_cast<void>(CE::Audio::Miniaudio::decode_file(fixture.file(), {0})), CE::Exceptions::invalid_args);
    EXPECT_THROW(static_cast<void>(CE::Audio::Miniaudio::decode_file({})), CE::Exceptions::invalid_args);
    const std::filesystem::path nul_path(std::string("sound\0.wav", 10));
    EXPECT_THROW(static_cast<void>(CE::Audio::Miniaudio::decode_file(nul_path)), CE::Exceptions::invalid_args);
    EXPECT_THROW(static_cast<void>(CE::Audio::Miniaudio::decode_file(fixture.file().parent_path() / "missing.wav")),
        CE::Exceptions::runtime_exception);
    {
        std::ofstream corrupt(fixture.file(), std::ios::binary | std::ios::trunc);
        corrupt.exceptions(std::ios::failbit | std::ios::badbit);
        corrupt << "not an audio file";
        corrupt.close();
    }
    EXPECT_THROW(static_cast<void>(CE::Audio::Miniaudio::decode_file(fixture.file())), CE::Exceptions::runtime_exception);
}

TEST(audio_decode, codecs) {
    for (const char* name : {"tones.flac", "tones.mp3"}) {
        const auto file = std::filesystem::path(CHERYL_AUDIO_TEST_FIXTURES) / name;
        const auto clip = CE::Audio::Miniaudio::decode_file(file);
        ASSERT_EQ(clip.format(), (CE::Audio::Format{2, 48000}));
        ASSERT_GT(clip.frame_count(), 45000);
        ASSERT_LT(clip.frame_count(), 53000);
        EXPECT_TRUE(std::ranges::all_of(clip.samples(), [](float value) { return std::isfinite(value); }));
        // Ignore codec priming and end padding; inspect a central 0.8-second span.
        for (std::size_t channel = 0; channel < 2; ++channel) {
            double power = 0;
            unsigned int crossings = 0;
            constexpr std::size_t first = 4800;
            constexpr std::size_t last = 43200;
            for (std::size_t frame = first; frame < last; ++frame) {
                const float sample = clip.samples()[frame * 2 + channel];
                power += static_cast<double>(sample) * sample;
                if (clip.samples()[(frame - 1) * 2 + channel] < 0 && sample >= 0)
                    ++crossings;
            }
            EXPECT_NEAR(std::sqrt(power / (last - first)), 0.25 / std::sqrt(2.0), 0.02);
            EXPECT_NEAR(crossings / 0.8, channel == 0 ? 440 : 660, 5);
        }
        if (file.extension() == ".flac") {
            EXPECT_EQ(clip.frame_count(), 48000);
            for (std::size_t frame = 0; frame < 256; ++frame) {
                for (std::size_t channel = 0; channel < 2; ++channel) {
                    const double frequency = channel == 0 ? 440 : 660;
                    const auto expected = 0.25 * std::sin(2 * std::numbers::pi * frequency * frame / 48000);
                    EXPECT_NEAR(clip.samples()[frame * 2 + channel], expected, 1.0 / 32768);
                }
            }
        }
    }
}

TEST(audio_stream, controls) {
    AudioFixture::Wav fixture;
    fixture.write({2, 48000}, std::vector<float>(2048, 0.25f));
    auto system = System::open_offline();
    auto voice = system->stream(fixture.file(), {.paused = true});
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Paused);
    ASSERT_TRUE(voice->snapshot().duration);
    EXPECT_NEAR(voice->snapshot().duration->count(), 1024.0 / 48000, 0.00001);
    voice->resume();
    std::array<float, 128> output{};
    system->render(output);
    expect_samples(output, 0.25f);
    voice->stop();
    system->render(output);
    expect_samples(output, 0);
    voice->restart();
    system->render(output);
    expect_samples(output, 0.25f);
    system->close();
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Closed);
}

TEST(audio_stream, failure) {
    AudioFixture::Wav fixture;
    auto system = System::open_offline();
    auto voice = system->play(constant());
    EXPECT_THROW(static_cast<void>(system->stream(fixture.file())), CE::Exceptions::runtime_exception);
    EXPECT_THROW(static_cast<void>(system->stream({})), CE::Exceptions::invalid_args);
    std::array<float, 64> output{};
    system->render(output);
    expect_samples(output, 0.25f);
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Playing);
    fixture.write({2, 48000}, std::vector<float>(2048, 0.125f));
    auto stream = system->stream(fixture.file(), {.paused = true});
    EXPECT_TRUE(std::filesystem::remove(fixture.file()));
    EXPECT_THROW(stream->restart(), CE::Exceptions::runtime_exception);
    EXPECT_EQ(stream->snapshot().state, PlaybackState::Paused);
    EXPECT_EQ(voice->snapshot().state, PlaybackState::Playing);
}
