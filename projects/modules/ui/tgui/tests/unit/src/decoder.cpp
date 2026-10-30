#include <gtest/gtest.h>

#include "../../../private/image-decoder.h"

#include <TGUI/Loading/ImageLoader.hpp>

#include <algorithm>
#include <array>
#include <new>

namespace {
    bool image_released = false;
    constexpr std::array<std::uint8_t, 4> rgba{11, 22, 33, 44};

    void release(void* owner) noexcept {
        *static_cast<bool*>(owner) = true;
    }

    CherylTguiPrivate::DecodeStatus valid(const std::uint8_t*, std::size_t, CherylTguiPrivate::DecodedImage& image) noexcept {
        image = {rgba.data(), rgba.size(), 1, 1, &image_released, release};
        return CherylTguiPrivate::DecodeStatus::Success;
    }

    CherylTguiPrivate::DecodeStatus invalid(const std::uint8_t* data, std::size_t bytes, CherylTguiPrivate::DecodedImage& image) noexcept {
        static_cast<void>(valid(data, bytes, image));
        image.bytes = 3;
        return CherylTguiPrivate::DecodeStatus::Success;
    }

    CherylTguiPrivate::DecodeStatus
    exhausted(const std::uint8_t* data, std::size_t bytes, CherylTguiPrivate::DecodedImage& image) noexcept {
        static_cast<void>(valid(data, bytes, image));
        return CherylTguiPrivate::DecodeStatus::OutOfMemory;
    }

    struct DecoderLease {
        const CherylTguiPrivate::ImageDecoder current;
        const CherylTguiPrivate::ImageDecoder previous;

        explicit DecoderLease(const CherylTguiPrivate::ImageDecoder decoder)
        : current(decoder), previous(cheryl_tgui_exchange_image_decoder_v1(decoder)) {
            image_released = false;
        }

        ~DecoderLease() { static_cast<void>(cheryl_tgui_restore_image_decoder_v1(current, previous)); }
    };
}

TEST(ui_tgui_decoder, callback) {
    const DecoderLease decoder(valid);
    tgui::Vector2u size;
    const auto pixels = tgui::ImageLoader::loadFromMemory(rgba.data(), rgba.size(), size);
    ASSERT_TRUE(pixels);
    EXPECT_EQ(size, (tgui::Vector2u{1, 1}));
    EXPECT_TRUE(std::equal(rgba.begin(), rgba.end(), pixels.get()));
    EXPECT_TRUE(image_released);
}

TEST(ui_tgui_decoder, invalid_cleanup) {
    const DecoderLease decoder(invalid);
    tgui::Vector2u size{9, 8};
    EXPECT_FALSE(tgui::ImageLoader::loadFromMemory(rgba.data(), rgba.size(), size));
    EXPECT_TRUE(image_released);
    EXPECT_EQ(size, (tgui::Vector2u{9, 8}));
}

TEST(ui_tgui_decoder, allocation_cleanup) {
    const DecoderLease decoder(exhausted);
    tgui::Vector2u size;
    EXPECT_THROW(static_cast<void>(tgui::ImageLoader::loadFromMemory(rgba.data(), rgba.size(), size)), std::bad_alloc);
    EXPECT_TRUE(image_released);
}
