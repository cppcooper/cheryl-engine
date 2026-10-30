#include <cheryl/assets/resources/decoded-image.h>

#include "../private/image-decoder.h"

#include <memory>
#include <new>

namespace {
    void release_image(void* owner) noexcept {
        delete static_cast<CE::Assets::Images::Decoded*>(owner);
    }

    CherylTguiPrivate::DecodeStatus
    decode_image(const std::uint8_t* data, const std::size_t bytes, CherylTguiPrivate::DecodedImage& image) noexcept {
        try {
            auto decoded =
                std::make_unique<CE::Assets::Images::Decoded>(CE::Assets::Images::decode(std::span<const unsigned char>{data, bytes}));
            image = {decoded->rgba.data(), decoded->rgba.size(), decoded->size.width, decoded->size.height, decoded.get(), release_image};
            static_cast<void>(decoded.release());
            return CherylTguiPrivate::DecodeStatus::Success;
        } catch (const std::bad_alloc&) {
            return CherylTguiPrivate::DecodeStatus::OutOfMemory;
        } catch (...) {
            return CherylTguiPrivate::DecodeStatus::Invalid;
        }
    }

    struct ImageDecoderRegistration {
        CherylTguiPrivate::ImageDecoder previous = cheryl_tgui_exchange_image_decoder_v1(decode_image);

        ~ImageDecoderRegistration() { static_cast<void>(cheryl_tgui_restore_image_decoder_v1(decode_image, previous)); }
    };

    ImageDecoderRegistration decoder_registration;
}

extern "C" CE_UI_TGUI_API void cheryl_tgui_retain_image_loader() {
    static_cast<void>(decoder_registration);
}
