#include <cheryl/backends/miniaudio.h>

#if defined(miniaudio_h) || defined(GLFW_VERSION_MAJOR) || defined(GL_VERSION_3_3)
    #error Audio consumers must not inherit native audio or graphics SDK headers.
#endif

#include "wav.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <future>
#include <iostream>
#include <numbers>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <utility>

namespace {
    using CE::Audio::Clip;
    using CE::Audio::PlaybackState;
    using CE::Audio::Miniaudio::System;

    int offline() {
        std::cout << "Audio consumer: checking offline PCM and closed handles.\n" << std::flush;
        auto system = CE::Audio::Miniaudio::System::open_offline();
        const auto clip = CE::Audio::Clip::from_samples({2, 48000}, std::vector<float>(2048, 0.25f));
        auto voice = system->play(clip, {.volume = 0.5f});
        std::array<float, 128> output{};
        system->render(output);
        const auto mismatch = std::ranges::find_if(output, [](float sample) { return !(std::abs(sample - 0.125f) < 0.00001f); });
        if (mismatch != output.end()) {
            std::cerr << "Audio consumer: offline sample " << mismatch - output.begin()
                      << " expected 0.125, got " << *mismatch << ".\n";
            return 1;
        }
        system->close();
        if (voice->snapshot().state != CE::Audio::PlaybackState::Closed) {
            std::cerr << "Audio consumer: surviving voice did not report Closed after system closure.\n";
            return 1;
        }
        std::cout << "Audio consumer: offline PCM and closed-handle checks completed.\n";
        return 0;
    }

    const char* state_name(PlaybackState state) {
        switch (state) {
            case PlaybackState::Playing:
                return "Playing";
            case PlaybackState::Paused:
                return "Paused";
            case PlaybackState::Finished:
                return "Finished";
            case PlaybackState::Stopped:
                return "Stopped";
            case PlaybackState::Closed:
                return "Closed";
        }
        return "Unknown";
    }

    Clip tone(double frequency) {
        constexpr std::size_t frames = 9600;
        std::vector<float> samples(frames * 2);
        for (std::size_t frame = 0; frame < frames; ++frame) {
            const double envelope = std::min({1.0, frame / 480.0, (frames - frame - 1) / 480.0});
            const auto value = static_cast<float>(0.25 * envelope * std::sin(2 * std::numbers::pi * frequency * frame / 48000));
            samples[frame * 2] = value;
            samples[frame * 2 + 1] = value;
        }
        return Clip::from_samples({2, 48000}, std::move(samples));
    }

    void write_music(const AudioFixture::Wav& fixture) {
        constexpr std::size_t rate = 48000;
        std::vector<float> samples(rate * 20 * 2);
        for (std::size_t frame = 0; frame < rate * 20; ++frame) {
            const auto second = frame / rate;
            const auto local_frame = frame % rate;
            const auto frequency = 220.0 * (1 + second % 4);
            const double envelope = std::min({1.0, local_frame / 480.0, (rate - local_frame - 1) / 480.0});
            const auto value = static_cast<float>(0.25 * envelope * std::sin(2 * std::numbers::pi * frequency * frame / rate));
            samples[frame * 2 + second % 2] = value;
        }
        fixture.write({2, 48000}, samples);
    }

    int device(bool producers, const std::optional<std::filesystem::path>& source) {
        AudioFixture::Wav generated;
        if (!source)
            write_music(generated);
        const auto file = source.value_or(generated.file());
        const std::array<Clip, 3> effects{tone(440), tone(660), tone(880)};
        auto system = System::open_device();
        system->set_volume(0.5f);
        auto music = system->stream(file, {.volume = 0.5f});

        // Keep discarded one-shots maintained even while stdin is waiting. Optional
        // effects are submitted from this producer while commands run on main.
        std::promise<void> result;
        auto completion = result.get_future();
        std::jthread worker([&system, &result, effects, producers](std::stop_token stop) {
            try {
                unsigned int ticks = 0;
                while (!stop.stop_requested()) {
                    system->maintain();
                    if (producers && ticks++ % 20 == 0)
                        static_cast<void>(system->play(effects[0], {.volume = 0.3f}));
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                result.set_value();
            } catch (...) {
                result.set_exception(std::current_exception());
            }
        });

        const auto output = system->output_format();
        std::cout << "Native backend: " << system->backend_name() << ", " << output.channels << " channels, "
            << output.sample_rate << " Hz\nStreaming: " << file << '\n';
        if (!source)
            std::cout << "Generated 20-second stream: 220 Hz left, 440 Hz right, 660 Hz left, 880 Hz right; repeats every four seconds.\n";
        if (producers)
            std::cout << "A background producer adds a short two-channel effect every two seconds.\n";
        std::cout << "Commands: p pause, r resume, s stop, x restart, l toggle loop, e overlapping effects,\n"
            << "          v VALUE music volume, m VALUE master volume, t status, q close.\n";

        std::string command;
        while (std::cout << "> " && std::getline(std::cin, command)) {
            if (command.empty())
                continue;
            if (command == "q")
                break;
            try {
                if (command == "p")
                    music->pause();
                else if (command == "r")
                    music->resume();
                else if (command == "s")
                    music->stop();
                else if (command == "x")
                    music->restart();
                else if (command == "l")
                    music->set_looping(!music->snapshot().looping);
                else if (command == "e") {
                    for (const auto& effect : effects)
                        static_cast<void>(system->play(effect, {.volume = 0.3f}));
                } else if (command[0] == 'v' || command[0] == 'm') {
                    std::istringstream parser(command.substr(1));
                    float volume = 0;
                    if (!(parser >> volume) || !(parser >> std::ws).eof())
                        throw std::invalid_argument("Volume requires one number within [0, 1]");
                    if (command[0] == 'v')
                        music->set_volume(volume);
                    else
                        system->set_volume(volume);
                } else if (command != "t") {
                    std::cout << "Unknown command.\n";
                    continue;
                }
                const auto snapshot = music->snapshot();
                std::cout << state_name(snapshot.state) << ", loop=" << snapshot.looping << ", music=" << snapshot.volume
                    << ", master=" << system->volume() << '\n';
            } catch (const std::exception& error) {
                std::cerr << "Audio command failed: " << error.what() << '\n';
            }
        }
        worker.request_stop();
        worker.join();
        completion.get();
        system->close();
        const auto final_state = music->snapshot().state;
        system.reset();
        std::cout << "Output closed; surviving music handle: " << state_name(final_state) << '\n';
        return final_state == PlaybackState::Closed && music->snapshot().state == PlaybackState::Closed ? 0 : 1;
    }
}

int main(int argc, char** argv) {
    try {
        bool native = false;
        bool producers = false;
        std::optional<std::filesystem::path> file;
        for (int index = 1; index < argc; ++index) {
            const std::string_view argument(argv[index]);
            if (argument == "--device")
                native = true;
            else if (argument == "--offline")
                native = false;
            else if (argument == "--producers")
                producers = true;
            else if (argument.starts_with("--stream="))
                file = std::string(argument.substr(9));
            else if (argument == "--help") {
                std::cout << "Audio consumer: --offline (default), or --device [--producers] [--stream=FILE].\n";
                return 0;
            } else {
                throw std::invalid_argument("Unknown audio consumer option");
            }
        }
        if (!native && (producers || file))
            throw std::invalid_argument("--producers and --stream require --device");
        return native ? device(producers, file) : offline();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
