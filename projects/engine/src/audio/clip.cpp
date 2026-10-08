#include <audio/clip.h>
#include <internals/exceptions.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace CE::Audio {
    struct Clip::Storage {
        Format format;
        std::vector<float> samples;
    };

    void Format::validate() const {
        if ((channels != 1 && channels != 2) || sample_rate < 8000 || sample_rate > 192000)
            throw Exceptions::invalid_args(CE_HERE, "Audio requires mono/stereo PCM at 8–192 kHz");
    }

    Clip::Clip(std::shared_ptr<const Storage> storage) : storage_(std::move(storage)) {}

    Clip Clip::from_samples(Format format, std::vector<float> samples) {
        format.validate();
        if (samples.empty() || samples.size() % format.channels != 0)
            throw Exceptions::invalid_args(CE_HERE, "Audio samples require nonempty complete frames");
        if (!std::ranges::all_of(samples, [](float value) { return std::isfinite(value); }))
            throw Exceptions::invalid_args(CE_HERE, "Audio samples must be finite");
        return Clip(std::make_shared<const Storage>(Storage{format, std::move(samples)}));
    }

    Format Clip::format() const noexcept { return storage_->format; }

    std::uint64_t Clip::frame_count() const noexcept { return storage_->samples.size() / storage_->format.channels; }

    std::chrono::duration<double> Clip::duration() const noexcept {
        return std::chrono::duration<double>(static_cast<double>(frame_count()) / storage_->format.sample_rate);
    }

    std::span<const float> Clip::samples() const noexcept { return storage_->samples; }
}
