#pragma once

#include <compare>
#include <cstdint>

namespace CE::Assets {
    enum class ImageFilter { Nearest, Linear };
    enum class MipmapFilter { None, Nearest, Linear };
    enum class ImageWrap { ClampToEdge, Repeat, MirroredRepeat };
    enum class ImageAnisotropy { Disabled, MaximumSupported };

    struct SamplerOptions {
        ImageFilter minification = ImageFilter::Linear;
        ImageFilter magnification = ImageFilter::Linear;
        MipmapFilter mipmaps = MipmapFilter::Linear;
        ImageWrap wrap_u = ImageWrap::ClampToEdge;
        ImageWrap wrap_v = ImageWrap::ClampToEdge;
        // Disable anisotropy when exact nearest sampling is required.
        ImageAnisotropy anisotropy = ImageAnisotropy::MaximumSupported;

        auto operator<=>(const SamplerOptions&) const = default;
    };

    // CPU-only validation of the requested filtering and wrap modes.
    void validate_sampler_options(const SamplerOptions& options);

    // Immutable sampling in one backend domain. Bindings retain the sampler
    // independently of image pixels; final release follows backend retirement.
    class Sampler {
        const SamplerOptions options_;
        const float effective_anisotropy_;

    public:
        virtual ~Sampler() = default;
        [[nodiscard]] const SamplerOptions& options() const noexcept { return options_; }
        // MaximumSupported resolves to 1 when the backend lacks anisotropy.
        [[nodiscard]] float effective_anisotropy() const noexcept { return effective_anisotropy_; }
        // Zero-based unit; requires the sampler's live backend owner/context.
        virtual void bind(std::uint32_t unit) const = 0;

    protected:
        Sampler(SamplerOptions options, float effective_anisotropy);
    };
}
