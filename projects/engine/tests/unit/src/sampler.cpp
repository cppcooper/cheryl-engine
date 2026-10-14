#include <assets/resources/parameters.h>

#include <gtest/gtest.h>
#include <internals/exceptions.h>

#include <array>
#include <limits>
#include <memory>

#ifdef GL_VERSION_3_3
#error CE::Rendering::Resources::Image sampling must not include OpenGL.
#endif

namespace {
    using CE::Exceptions::invalid_args;

    class MemorySampler final : public CE::Rendering::Sampling::Sampler {
    public:
        MemorySampler(const CE::Rendering::Sampling::Options options, const float anisotropy)
        : CE::Rendering::Sampling::Sampler(options, anisotropy) {}
        void bind(std::uint32_t) const override {}
    };

    struct MemoryImage final : CE::Rendering::Resources::Image {
        CE::Rendering::Geometry::PixelSize pixel_size() const override { return {2, 2}; }
        void bind(std::uint32_t) const override {}
    };
}

TEST(image_sampling, immutable_options) {
    CE::Rendering::Sampling::Options requested;
    const MemorySampler sampler(requested, 16);
    requested.magnification = CE::Rendering::Sampling::Filter::Nearest;
    EXPECT_EQ(sampler.options().magnification, CE::Rendering::Sampling::Filter::Linear);
    EXPECT_EQ(sampler.options().mipmaps, CE::Rendering::Sampling::MipmapFilter::Linear);
    EXPECT_EQ(sampler.options().anisotropy, CE::Rendering::Sampling::Anisotropy::MaximumSupported);
    EXPECT_FLOAT_EQ(sampler.effective_anisotropy(), 16);
    const MemorySampler fallback({}, 1);
    EXPECT_FLOAT_EQ(fallback.effective_anisotropy(), 1);
    EXPECT_EQ(fallback.options().anisotropy, CE::Rendering::Sampling::Anisotropy::MaximumSupported);
}

TEST(image_sampling, invalid_options) {
    std::array<CE::Rendering::Sampling::Options, 6> invalid{};
    invalid[0].minification = static_cast<CE::Rendering::Sampling::Filter>(-1);
    invalid[1].magnification = static_cast<CE::Rendering::Sampling::Filter>(2);
    invalid[2].mipmaps = static_cast<CE::Rendering::Sampling::MipmapFilter>(3);
    invalid[3].wrap_u = static_cast<CE::Rendering::Sampling::Wrap>(3);
    invalid[4].wrap_v = static_cast<CE::Rendering::Sampling::Wrap>(-1);
    invalid[5].anisotropy = static_cast<CE::Rendering::Sampling::Anisotropy>(2);
    for (const auto& options : invalid)
        EXPECT_THROW(static_cast<void>(MemorySampler(options, 1)), invalid_args);
    for (const float level : {0.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        EXPECT_THROW(static_cast<void>(MemorySampler({}, level)), invalid_args);
    CE::Rendering::Sampling::Options disabled;
    disabled.anisotropy = CE::Rendering::Sampling::Anisotropy::Disabled;
    EXPECT_THROW(static_cast<void>(MemorySampler(disabled, 2)), invalid_args);
    EXPECT_NO_THROW(static_cast<void>(MemorySampler(disabled, 1)));
}

TEST(image_sampling, retained_bindings) {
    const CE::Rendering::Parameters::Contract contract{
        {"first", CE::Rendering::Parameters::Type::Sampler2D}, {"second", CE::Rendering::Parameters::Type::Sampler2D}
    };
    auto image = std::make_shared<MemoryImage>();
    auto smooth = std::make_shared<MemorySampler>(CE::Rendering::Sampling::Options{}, 16);
    CE::Rendering::Sampling::Options nearest_options;
    nearest_options.minification = nearest_options.magnification = CE::Rendering::Sampling::Filter::Nearest;
    nearest_options.mipmaps = CE::Rendering::Sampling::MipmapFilter::None;
    nearest_options.anisotropy = CE::Rendering::Sampling::Anisotropy::Disabled;
    auto nearest = std::make_shared<MemorySampler>(nearest_options, 1);
    const std::weak_ptr<const CE::Rendering::Sampling::Sampler> smooth_lifetime = smooth;
    const std::weak_ptr<const CE::Rendering::Sampling::Sampler> nearest_lifetime = nearest;
    CE::Rendering::Parameters::Set authored{
        {"first", CE::Rendering::Parameters::ImageBinding{image, 0, smooth}},
        {"second", CE::Rendering::Parameters::ImageBinding{image, 3, nearest}}
    };
    auto retained = CE::Rendering::Parameters::resolve(contract, {}, {}, {}, authored, {});
    const auto defaults =
        CE::Rendering::Parameters::resolve(contract, {}, {}, {}, authored, {{"first", CE::Rendering::Parameters::ImageBinding{image, 0}}});
    EXPECT_FALSE(std::get<CE::Rendering::Parameters::ImageBinding>(defaults.at("first")).sampler);
    authored.clear();
    smooth.reset();
    nearest.reset();
    image.reset();
    const auto& first = std::get<CE::Rendering::Parameters::ImageBinding>(retained.at("first"));
    const auto& second = std::get<CE::Rendering::Parameters::ImageBinding>(retained.at("second"));
    EXPECT_EQ(first.image, second.image);
    ASSERT_TRUE(first.sampler);
    ASSERT_TRUE(second.sampler);
    EXPECT_NE(first.sampler, second.sampler);
    EXPECT_EQ(first.sampler->options().magnification, CE::Rendering::Sampling::Filter::Linear);
    EXPECT_EQ(second.sampler->options(), nearest_options);
    EXPECT_FALSE(smooth_lifetime.expired());
    retained.clear();
    EXPECT_TRUE(smooth_lifetime.expired());
    // The second binding is also retained by the independently resolved defaults.
    EXPECT_FALSE(nearest_lifetime.expired());
}
