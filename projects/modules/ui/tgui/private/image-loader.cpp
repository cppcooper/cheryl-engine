#include "image-decoder.h"

#include <TGUI/Loading/ImageLoader.hpp>
#include <TGUI/Global.hpp>

#include <algorithm>
#include <atomic>
#include <limits>
#include <new>

namespace {
    std::atomic<CherylTguiPrivate::ImageDecoder> image_decoder{nullptr};

    struct ImageLease {
        CherylTguiPrivate::DecodedImage image;

        ~ImageLease() {
            if (image.release)
                image.release(image.owner);
        }
    };
}

extern "C" CherylTguiPrivate::ImageDecoder cheryl_tgui_exchange_image_decoder_v1(const CherylTguiPrivate::ImageDecoder decoder) noexcept {
    return image_decoder.exchange(decoder);
}

extern "C" bool
cheryl_tgui_restore_image_decoder_v1(CherylTguiPrivate::ImageDecoder expected, const CherylTguiPrivate::ImageDecoder replacement) noexcept {
    return image_decoder.compare_exchange_strong(expected, replacement);
}

namespace tgui {
    std::unique_ptr<std::uint8_t[]> ImageLoader::loadFromFile(const String& filename, Vector2u& image_size) {
        std::size_t bytes{};
        const auto data = readFileToMemory(filename, bytes);
        return data ? loadFromMemory(data.get(), bytes, image_size) : nullptr;
    }

    std::unique_ptr<std::uint8_t[]> ImageLoader::loadFromMemory(const std::uint8_t* data, const std::size_t bytes, Vector2u& image_size) {
        const auto decode = image_decoder.load();
        if (!data || !bytes || !decode)
            return {};
        ImageLease lease;
        const auto status = decode(data, bytes, lease.image);
        if (status == CherylTguiPrivate::DecodeStatus::OutOfMemory)
            throw std::bad_alloc{};
        const auto& image = lease.image;
        if (status != CherylTguiPrivate::DecodeStatus::Success || !image.pixels || !image.width || !image.height ||
            image.width > (std::numeric_limits<std::size_t>::max)() / 4 / image.height ||
            image.bytes != static_cast<std::size_t>(image.width) * image.height * 4)
            return {};
        std::unique_ptr<std::uint8_t[]> pixels{new std::uint8_t[image.bytes]};
        std::copy_n(image.pixels, image.bytes, pixels.get());
        image_size = {image.width, image.height};
        return pixels;
    }
}
