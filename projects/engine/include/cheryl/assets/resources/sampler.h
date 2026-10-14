#pragma once

#include <compare>
#include <cstdint>

namespace CE::Rendering::Sampling {
    enum class Filter { Nearest, Linear };
    enum class MipmapFilter { None, Nearest, Linear };
    enum class Wrap { ClampToEdge, Repeat, MirroredRepeat };
    enum class Anisotropy { Disabled, MaximumSupported };

    struct Options {
        Filter minification = Filter::Linear;
        Filter magnification = Filter::Linear;
        MipmapFilter mipmaps = MipmapFilter::Linear;
        Wrap wrap_u = Wrap::ClampToEdge;
        Wrap wrap_v = Wrap::ClampToEdge;
        // Disable anisotropy when exact nearest sampling is required.
        Anisotropy anisotropy = Anisotropy::MaximumSupported;

        auto operator<=>(const Options&) const = default;
    };

    // CPU-only validation of the requested filtering and wrap modes.
    void validate(const Options& options);

    // Immutable sampling in one backend domain. Bindings retain the sampler
    // independently of image pixels; final release follows backend retirement.
    class Sampler {
        const Options options_;
        const float effective_anisotropy_;

    public:
        virtual ~Sampler() = default;
        [[nodiscard]] const Options& options() const noexcept { return options_; }
        // MaximumSupported resolves to 1 when the backend lacks anisotropy.
        [[nodiscard]] float effective_anisotropy() const noexcept { return effective_anisotropy_; }
        // Zero-based unit; requires the sampler's live backend owner/context.
        virtual void bind(std::uint32_t unit) const = 0;

    protected:
        Sampler(Options options, float effective_anisotropy);
    };
}
