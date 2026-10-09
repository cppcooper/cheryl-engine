#include <assets/resources/sampler.h>

#include <internals/exceptions.h>

#include <cmath>

namespace CE::Assets {
    void validate_sampler_options(const SamplerOptions& options) {
        const auto valid_filter = [](const ImageFilter filter) { return filter == ImageFilter::Nearest || filter == ImageFilter::Linear; };
        const auto valid_wrap = [](const ImageWrap wrap) {
            return wrap == ImageWrap::ClampToEdge || wrap == ImageWrap::Repeat || wrap == ImageWrap::MirroredRepeat;
        };
        if (!valid_filter(options.minification) || !valid_filter(options.magnification) ||
            (options.mipmaps != MipmapFilter::None && options.mipmaps != MipmapFilter::Nearest && options.mipmaps != MipmapFilter::Linear) ||
            !valid_wrap(options.wrap_u) || !valid_wrap(options.wrap_v) ||
            (options.anisotropy != ImageAnisotropy::Disabled && options.anisotropy != ImageAnisotropy::MaximumSupported))
            throw Exceptions::invalid_args(CE_HERE, "Unknown image sampling mode");
    }

    Sampler::Sampler(const SamplerOptions options, const float effective_anisotropy)
    : options_(options), effective_anisotropy_(effective_anisotropy) {
        validate_sampler_options(options);
        if (!std::isfinite(effective_anisotropy) || effective_anisotropy < 1 ||
            (options.anisotropy == ImageAnisotropy::Disabled && effective_anisotropy != 1))
            throw Exceptions::invalid_args(CE_HERE, "Invalid effective image anisotropy");
    }
}
