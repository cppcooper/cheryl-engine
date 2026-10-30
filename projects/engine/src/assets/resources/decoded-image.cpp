#include <assets/resources/decoded-image.h>
#include <internals/exceptions.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <limits>
#include <memory>

namespace CE::Assets::Images {
    namespace {
        Decoded decoded(unsigned char* data, const int width, const int height) {
            std::unique_ptr<unsigned char, decltype(&stbi_image_free)> pixels(data, &stbi_image_free);
            if (!pixels || width <= 0 || height <= 0)
                throw Exceptions::runtime_exception(CE_HERE, "Could not decode image");
            const auto w = static_cast<std::size_t>(width);
            const auto h = static_cast<std::size_t>(height);
            if (w > std::numeric_limits<std::size_t>::max() / 4 || h > std::numeric_limits<std::size_t>::max() / (w * 4))
                throw Exceptions::invalid_args(CE_HERE, "Decoded image exceeds addressable storage");
            const auto bytes = w * h * 4;
            return {
                {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)},
                std::vector<unsigned char>(pixels.get(), pixels.get() + bytes)
            };
        }
    }

    Decoded decode(const std::filesystem::path& file) {
        int width = 0, height = 0, channels = 0;
        auto* pixels = stbi_load(file.string().c_str(), &width, &height, &channels, 4);
        if (!pixels)
            throw Exceptions::runtime_exception(CE_HERE, "Could not decode image: " + file.string());
        return decoded(pixels, width, height);
    }

    Decoded decode(const std::span<const unsigned char> bytes) {
        if (bytes.empty() || bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            throw Exceptions::invalid_args(CE_HERE, "Encoded image exceeds decoder input limits");
        int width = 0, height = 0, channels = 0;
        auto* pixels = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height, &channels, 4);
        return decoded(pixels, width, height);
    }
}
