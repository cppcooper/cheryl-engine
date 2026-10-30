#pragma once

#include <cheryl/export.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace CE::Audio {
    /** Interleaved PCM format supported by the neutral audio contract: mono/stereo at 8..192kHz. */
    struct Format {
        std::uint32_t channels = 2; ///< Samples per frame; only 1(mono) or 2(stereo) are supported.
        std::uint32_t sample_rate = 48000; ///< Frames per second, inclusive 8000..192000.

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

/** @fn void CE::Audio::Format::validate() const
 * Reject unsupported channel count or sample rate with invalid_args; no device or allocation is required.
 */

/** @fn bool CE::Audio::Format::operator==(const Format&) const
 * Compare channel count and sample rate by value.
 */

/** @fn CE::Audio::Clip::Clip(const Clip&)
 * Share immutable PCM storage; neither a device nor the source Clip lifetime is retained.
 */

/** @fn CE::Audio::Clip& CE::Audio::Clip::operator=(const Clip&)
 * Replace this handle with shared immutable source storage. Assignment does not alter PCM already retained by voices.
 */

/** @fn CE::Audio::Clip CE::Audio::Clip::from_samples(Format format, std::vector<float> samples)
 * Consume/copy owned float PCM at the supplied format. Reject empty data, incomplete interleaved frames, unsupported format and nonfinite samples with invalid_args. Nominal amplitude is[-1,1], without implicit clamping. Allocation failures propagate; no audio system/device is opened.
 * @see [Audio preparation](cheryl-guide:modules/miniaudio#preparation-and-output)
 */

/** @fn CE::Audio::Format CE::Audio::Clip::format() const noexcept
 * Return a copied channel/sample-rate format.
 */

/** @fn std::uint64_t CE::Audio::Clip::frame_count() const noexcept
 * Return complete PCM frame count: samples().size()/format().channels.
 */

/** @fn std::chrono::duration<double> CE::Audio::Clip::duration() const noexcept
 * Return clip duration in seconds, frame_count()/sample_rate.
 */

/** @fn std::span<const float> CE::Audio::Clip::samples() const noexcept
 * Borrow immutable channel-interleaved float samples. Retain a Clip copy while using the view; it does not borrow the original input vector.
 */
