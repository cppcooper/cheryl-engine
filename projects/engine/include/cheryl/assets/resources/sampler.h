#pragma once

#include <cheryl/export.h>

#include <compare>
#include <cstdint>

namespace CE::Rendering::Sampling {
    /** Texel filtering within a mip level. */
    enum class Filter {
        Nearest, ///< Choose the nearest texel in a level.
        Linear ///< Interpolate neighboring texels in a level.
    };
    /** Mip-level policy: base level only, nearest level, or interpolation between levels. */
    enum class MipmapFilter {
        None, ///< Sample only the base level.
        Nearest, ///< Choose the nearest mip level.
        Linear ///< Interpolate between neighboring mip levels.
    };
    /** Addressing outside normalized texture coordinates. */
    enum class Wrap {
        ClampToEdge, ///< Extend the edge texel outside the image.
        Repeat, ///< Repeat the normalized image interval.
        MirroredRepeat ///< Repeat with alternating reflected intervals.
    };
    /** Request isotropic filtering or the backend's supported anisotropy maximum. */
    enum class Anisotropy {
        Disabled, ///< Use isotropic filtering, including exact nearest requests.
        MaximumSupported ///< Resolve to the supported maximum, or 1 when unavailable.
    };

    /** Immutable sampling request; aggregate construction alone performs no validation. */
    struct Options {
        Filter minification = Filter::Linear; ///< Filter when the image occupies fewer pixels.
        Filter magnification = Filter::Linear; ///< Filter when the image occupies more pixels.
        MipmapFilter mipmaps = MipmapFilter::Linear; ///< Mip-level selection; native validation also checks image completeness.
        Wrap wrap_u = Wrap::ClampToEdge; ///< Horizontal normalized-coordinate addressing.
        Wrap wrap_v = Wrap::ClampToEdge; ///< Vertical normalized-coordinate addressing.
        // Disable anisotropy when exact nearest sampling is required.
        Anisotropy anisotropy = Anisotropy::MaximumSupported; ///< MaximumSupported may resolve to isotropic filtering.

        auto operator<=>(const Options&) const = default;
    };

    // CPU-only validation of the requested filtering and wrap modes.
    CE_API void validate(const Options& options);

    /** Immutable sampling object retained independently of pixel contents.
     * @see [Rendering guide](cheryl-guide:rendering/pipelines-and-materials)
     */
    // Immutable sampling in one backend domain. Bindings retain the sampler
    // independently of image pixels; final release follows backend retirement.
    class CE_API Sampler {
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

/**
 * @fn void CE::Rendering::Sampling::validate(const Options& options)
 * @brief Reject unknown filter, mipmap, wrap or anisotropy enum values.
 * @param options CPU request borrowed for the call.
 * @throws CE::Exceptions::invalid_args For unsupported enum values. No backend capabilities are queried.
 */

/**
 * @fn CE::Rendering::Sampling::Sampler::~Sampler()
 * @brief Release logical sampler ownership; native cleanup follows the backend retirement contract.
 */

/**
 * @fn const CE::Rendering::Sampling::Options& CE::Rendering::Sampling::Sampler::options() const noexcept
 * @return Immutable requested options borrowed from this sampler; retain it while using the reference.
 */

/**
 * @fn float CE::Rendering::Sampling::Sampler::effective_anisotropy() const noexcept
 * @return Resolved backend anisotropy factor; 1 denotes isotropic filtering, including unsupported MaximumSupported requests.
 */

/**
 * @fn void CE::Rendering::Sampling::Sampler::bind(std::uint32_t unit) const
 * @brief Apply sampling to a zero-based texture unit on the live backend owner/current context.
 * @param unit Unit supported by that backend.
 * Retain this sampler and its original domain through use; binding does not replace image pixels or select a material.
 */

/**
 * @fn CE::Rendering::Sampling::Sampler::Sampler(Options options, float effective_anisotropy)
 * @brief Initialize immutable requested and resolved sampling metadata for an adapter.
 * @param options Valid sampling request.
 * @param effective_anisotropy Finite backend result at least 1; Disabled requires exactly 1.
 * Malformed options or effective factors throw invalid_args before returning a sampler.
 */

/**
 * @fn auto CE::Rendering::Sampling::Options::operator<=>(const Options&) const
 * @brief Compare all requested policy fields lexicographically using their declared enum order. This compares requests, not resolved backend anisotropy or resource identity.
 */
