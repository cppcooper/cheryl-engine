#pragma once

#include <cheryl/export.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace CE::Audio {
    struct Format {
        std::uint32_t channels = 2;
        std::uint32_t sample_rate = 48000;

        // Initial scope: mono/stereo, 8–192 kHz. Throws for unsupported values.
        CE_API void validate() const;
        bool operator==(const Format&) const = default;
    };

    /** Immutable, owned interleaved float PCM. Frames contain one sample per channel.
     * Samples must be finite; [-1, 1] is the nominal range, without implicit clamping.
     * Copies share storage. Moving also preserves the source's valid snapshot.
     * Neither construction nor access requires an audio system or native device.
     */
    class Clip final {
        struct Storage;
        std::shared_ptr<const Storage> storage_;

        CE_API explicit Clip(std::shared_ptr<const Storage> storage);

    public:
        Clip(const Clip&) = default;
        Clip& operator=(const Clip&) = default;

        // Rejects empty data, incomplete frames and non-finite samples.
        [[nodiscard]] CE_API static Clip from_samples(::CE::Audio::Format format, std::vector<float> samples);
        [[nodiscard]] CE_API ::CE::Audio::Format format() const noexcept;
        [[nodiscard]] CE_API std::uint64_t frame_count() const noexcept;
        [[nodiscard]] CE_API std::chrono::duration<double> duration() const noexcept;
        [[nodiscard]] CE_API std::span<const float> samples() const noexcept;
    };
}
