#pragma once

#include <audio/clip.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>

namespace AudioFixture {
    class Wav final {
        std::filesystem::path directory_;
        std::filesystem::path file_;

        static void u16(std::ostream& output, std::uint16_t value) {
            const char bytes[]{static_cast<char>(value & 255), static_cast<char>(value >> 8)};
            output.write(bytes, 2);
        }

        static void u32(std::ostream& output, std::uint32_t value) {
            u16(output, static_cast<std::uint16_t>(value & 65535));
            u16(output, static_cast<std::uint16_t>(value >> 16));
        }

    public:
        Wav() {
            static std::atomic<unsigned int> next{0};
            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            const auto name = "cheryl-audio-" + std::to_string(stamp) + "-" + std::to_string(next.fetch_add(1));
            directory_ = std::filesystem::temp_directory_path() / name;
            file_ = directory_ / "sound.wav";
            if (!std::filesystem::create_directory(directory_))
                throw std::runtime_error("Cannot create isolated audio fixture directory");
        }

        ~Wav() {
            std::error_code error;
            static_cast<void>(std::filesystem::remove_all(directory_, error));
        }

        Wav(const Wav&) = delete;
        Wav& operator=(const Wav&) = delete;

        [[nodiscard]] const std::filesystem::path& file() const noexcept { return file_; }

        void write(CE::Audio::Format format, std::span<const float> samples) const {
            format.validate();
            if (samples.empty() || samples.size() % format.channels != 0
                || samples.size() > (std::numeric_limits<std::uint32_t>::max() - 36) / 2)
                throw std::runtime_error("Invalid audio fixture size");
            const auto bytes = static_cast<std::uint32_t>(samples.size() * 2);
            std::ofstream output(file_, std::ios::binary | std::ios::trunc);
            output.exceptions(std::ios::failbit | std::ios::badbit);
            output.write("RIFF", 4);
            u32(output, 36 + bytes);
            output.write("WAVEfmt ", 8);
            u32(output, 16);
            u16(output, 1);
            u16(output, static_cast<std::uint16_t>(format.channels));
            u32(output, format.sample_rate);
            u32(output, format.sample_rate * format.channels * 2);
            u16(output, static_cast<std::uint16_t>(format.channels * 2));
            u16(output, 16);
            output.write("data", 4);
            u32(output, bytes);
            for (const float sample : samples) {
                if (!std::isfinite(sample))
                    throw std::runtime_error("Non-finite audio fixture sample");
                const auto scaled = std::clamp(std::lround(std::clamp(sample, -1.0f, 1.0f) * 32768), -32768L, 32767L);
                u16(output, static_cast<std::uint16_t>(static_cast<std::int16_t>(scaled)));
            }
            output.close();
        }
    };
}
