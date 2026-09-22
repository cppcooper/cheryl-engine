#pragma once
#ifndef TEXTURE_H
#define TEXTURE_H

#include <assets/abstracts/image.h>

#include <cgl.h>
#include <filesystem>

namespace fs = std::filesystem;

namespace CE::Assets {
    struct Texture final : Image {
        GLuint id{};
        GLuint unit{};
        int32_t width{};
        int32_t height{};
        explicit Texture(const char* file, int slot, bool use_mipmaps, bool pixelate, int wrap_opt);
        explicit Texture(unsigned char* bitmap_data, int width, int height, uint32_t slot, bool use_mipmaps, bool pixelate, int wrap_opt = GL_CLAMP_TO_EDGE, GLenum fmt = GL_RED);
        [[nodiscard]] PixelSize pixel_size() const override {
            return {width > 0 ? static_cast<std::uint32_t>(width) : 0,
                    height > 0 ? static_cast<std::uint32_t>(height) : 0};
        }
        void bind() const;
        static void unbind();
    };
}
#endif
