#include <assets/resources/parameters.h>

#include <gtest/gtest.h>
#include <internals/exceptions.h>

#include <array>
#include <limits>
#include <memory>

#ifdef GL_VERSION_3_3
#error Image sampling must not include OpenGL.
#endif

namespace {
    using namespace CE::Assets;
    using CE::Exceptions::invalid_args;

    class MemorySampler final : public Sampler {
    public:
        MemorySampler(const SamplerOptions options, const float anisotropy)
        : Sampler(options, anisotropy) {}
        void bind(std::uint32_t) const override {}
    };

    struct MemoryImage final : Image {
        PixelSize pixel_size() const override { return {2, 2}; }
        void bind(std::uint32_t) const override {}
    };
}

TEST(image_sampling, immutable_options) {
    SamplerOptions requested;
    const MemorySampler sampler(requested, 16);
    requested.magnification = ImageFilter::Nearest;
    EXPECT_EQ(sampler.options().magnification, ImageFilter::Linear);
    EXPECT_EQ(sampler.options().mipmaps, MipmapFilter::Linear);
    EXPECT_EQ(sampler.options().anisotropy, ImageAnisotropy::MaximumSupported);
    EXPECT_FLOAT_EQ(sampler.effective_anisotropy(), 16);
    const MemorySampler fallback({}, 1);
    EXPECT_FLOAT_EQ(fallback.effective_anisotropy(), 1);
    EXPECT_EQ(fallback.options().anisotropy, ImageAnisotropy::MaximumSupported);
}

TEST(image_sampling, invalid_options) {
    std::array<SamplerOptions, 6> invalid{};
    invalid[0].minification = static_cast<ImageFilter>(-1);
    invalid[1].magnification = static_cast<ImageFilter>(2);
    invalid[2].mipmaps = static_cast<MipmapFilter>(3);
    invalid[3].wrap_u = static_cast<ImageWrap>(3);
    invalid[4].wrap_v = static_cast<ImageWrap>(-1);
    invalid[5].anisotropy = static_cast<ImageAnisotropy>(2);
    for (const auto& options : invalid)
        EXPECT_THROW(static_cast<void>(MemorySampler(options, 1)), invalid_args);
    for (const float level : {0.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        EXPECT_THROW(static_cast<void>(MemorySampler({}, level)), invalid_args);
    SamplerOptions disabled;
    disabled.anisotropy = ImageAnisotropy::Disabled;
    EXPECT_THROW(static_cast<void>(MemorySampler(disabled, 2)), invalid_args);
    EXPECT_NO_THROW(static_cast<void>(MemorySampler(disabled, 1)));
}

TEST(image_sampling, retained_bindings) {
    const ParameterContract contract{{"first", ParameterType::Sampler2D}, {"second", ParameterType::Sampler2D}};
    auto image = std::make_shared<MemoryImage>();
    auto smooth = std::make_shared<MemorySampler>(SamplerOptions{}, 16);
    SamplerOptions nearest_options;
    nearest_options.minification = nearest_options.magnification = ImageFilter::Nearest;
    nearest_options.mipmaps = MipmapFilter::None;
    nearest_options.anisotropy = ImageAnisotropy::Disabled;
    auto nearest = std::make_shared<MemorySampler>(nearest_options, 1);
    const std::weak_ptr<const Sampler> smooth_lifetime = smooth;
    const std::weak_ptr<const Sampler> nearest_lifetime = nearest;
    ParameterSet authored{{"first", ImageBinding{image, 0, smooth}}, {"second", ImageBinding{image, 3, nearest}}};
    auto retained = resolve_parameters(contract, {}, {}, {}, authored, {});
    const auto defaults = resolve_parameters(contract, {}, {}, {}, authored, {{"first", ImageBinding{image, 0}}});
    EXPECT_FALSE(std::get<ImageBinding>(defaults.at("first")).sampler);
    authored.clear();
    smooth.reset();
    nearest.reset();
    image.reset();
    const auto& first = std::get<ImageBinding>(retained.at("first"));
    const auto& second = std::get<ImageBinding>(retained.at("second"));
    EXPECT_EQ(first.image, second.image);
    ASSERT_TRUE(first.sampler);
    ASSERT_TRUE(second.sampler);
    EXPECT_NE(first.sampler, second.sampler);
    EXPECT_EQ(first.sampler->options().magnification, ImageFilter::Linear);
    EXPECT_EQ(second.sampler->options(), nearest_options);
    EXPECT_FALSE(smooth_lifetime.expired());
    retained.clear();
    EXPECT_TRUE(smooth_lifetime.expired());
    // The second binding is also retained by the independently resolved defaults.
    EXPECT_FALSE(nearest_lifetime.expired());
}
