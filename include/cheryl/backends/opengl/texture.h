#pragma once
#ifndef TEXTURE_H
#define TEXTURE_H

#include <assets/resources/image.h>

#include <backends/opengl/gl.h>
#include <backends/opengl/resource-lifetime.h>
#include <cstdint>
#include <memory>

namespace CE::Assets {
    struct Texture final : Image {
    private:
        RenderAPIs::OpenGLHandle handle_;
        std::uint32_t binding_unit_limit_ = 0;

    public:
        std::int32_t width{};
        std::int32_t height{};
        explicit Texture(
            std::shared_ptr<RenderAPIs::OpenGLResourceLifetime> lifetime,
            const char* file,
            bool use_mipmaps,
            bool pixelate,
            int wrap_opt
        );
        // Raw RGBA rows are top-to-bottom, matching DecodedImage. Red-only stb
        // atlas rows retain the order expected by stb's baked UV coordinates.
        explicit Texture(
            std::shared_ptr<RenderAPIs::OpenGLResourceLifetime> lifetime,
            const unsigned char* bitmap_data,
            int width,
            int height,
            bool use_mipmaps,
            bool pixelate,
            int wrap_opt = GL_CLAMP_TO_EDGE,
            GLenum fmt = GL_RED
        );
        [[nodiscard]] PixelSize pixel_size() const override {
            return {width > 0 ? static_cast<std::uint32_t>(width) : 0, height > 0 ? static_cast<std::uint32_t>(height) : 0};
        }
        void bind(std::uint32_t unit) const override;
        void require_binding(std::uint32_t unit) const;
        [[nodiscard]] const RenderAPIs::OpenGLResourceLifetime* resource_domain() const noexcept { return handle_.resource_domain(); }
        // Select the requested unit explicitly and require this texture's live context.
        void unbind(std::uint32_t unit) const;
    };
}
#endif
