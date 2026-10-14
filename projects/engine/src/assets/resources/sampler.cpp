#include <assets/resources/sampler.h>

#include <internals/exceptions.h>

#include <cmath>

namespace CE::Rendering::Sampling {
    void validate(const Options& options) {
        const auto valid_filter = [](const Filter filter) { return filter == Filter::Nearest || filter == Filter::Linear; };
        const auto valid_wrap = [](const Wrap wrap) {
            return wrap == Wrap::ClampToEdge || wrap == Wrap::Repeat || wrap == Wrap::MirroredRepeat;
        };
        if (!valid_filter(options.minification) || !valid_filter(options.magnification) ||
            (options.mipmaps != MipmapFilter::None && options.mipmaps != MipmapFilter::Nearest &&
             options.mipmaps != MipmapFilter::Linear) ||
            !valid_wrap(options.wrap_u) || !valid_wrap(options.wrap_v) ||
            (options.anisotropy != Anisotropy::Disabled && options.anisotropy != Anisotropy::MaximumSupported))
            throw Exceptions::invalid_args(CE_HERE, "Unknown image sampling mode");
    }

    Sampler::Sampler(const Options options, const float effective_anisotropy)
    : options_(options), effective_anisotropy_(effective_anisotropy) {
        validate(options);
        if (!std::isfinite(effective_anisotropy) || effective_anisotropy < 1 ||
            (options.anisotropy == Anisotropy::Disabled && effective_anisotropy != 1))
            throw Exceptions::invalid_args(CE_HERE, "Invalid effective image anisotropy");
    }
}
