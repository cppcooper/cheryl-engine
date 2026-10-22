#pragma once

#include <TGUI/Config.hpp>

#include <cstddef>
#include <cstdint>

namespace CherylTguiPrivate {
    enum class DecodeStatus : std::uint32_t { Success, Invalid, OutOfMemory };

    // The decoder retains allocation ownership. ImageLoader copies the pixels
    // into toolkit storage before invoking release, including on failure.
    struct DecodedImage {
        const std::uint8_t* pixels{};
        std::size_t bytes{};
        std::uint32_t width{};
        std::uint32_t height{};
        void* owner{};
        void (*release)(void*) noexcept {};
    };

    using ImageDecoder = DecodeStatus (*)(const std::uint8_t*, std::size_t, DecodedImage&) noexcept;
}

extern "C" {
TGUI_API CherylTguiPrivate::ImageDecoder cheryl_tgui_exchange_image_decoder_v1(CherylTguiPrivate::ImageDecoder decoder) noexcept;
TGUI_API bool
cheryl_tgui_restore_image_decoder_v1(CherylTguiPrivate::ImageDecoder expected, CherylTguiPrivate::ImageDecoder replacement) noexcept;
}
