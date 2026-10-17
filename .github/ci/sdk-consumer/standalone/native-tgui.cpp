#include <TGUI/Texture.hpp>
#include <TGUI/Loading/ImageLoader.hpp>

#include <array>
#include <cstdio>

namespace {
    using MemoryLoader = void (tgui::Texture::*)(const std::uint8_t*, std::size_t, const tgui::UIntRect&, const tgui::UIntRect&, bool);
    MemoryLoader volatile memory_loader = &tgui::Texture::loadFromMemory;
}

int main() {
    constexpr std::array<std::uint8_t, 21> image{
        0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 24, 0x20, 0, 0, 255
    };
    tgui::Vector2u size;
    const auto pixels = tgui::ImageLoader::loadFromMemory(image.data(), image.size(), size);
    if (!pixels) {
        std::fputs("Native TGUI consumer: image decoding failed.\n", stderr);
        return 1;
    }
    if (size != tgui::Vector2u{1, 1} || pixels[0] != 255 || pixels[1] != 0 || pixels[2] != 0 || pixels[3] != 255) {
        std::fputs("Native TGUI consumer: image decoding returned unexpected dimensions or pixels.\n", stderr);
        return 1;
    }
    tgui::Texture texture;
    texture.setColor(tgui::Color::Red);
    const tgui::Texture copy{texture};
    if (!memory_loader || copy.getColor() != tgui::Color::Red) {
        std::fputs("Native TGUI consumer: texture entry point or color copy check failed.\n", stderr);
        return 1;
    }
    return 0;
}
