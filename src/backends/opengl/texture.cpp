#include <backends/opengl/texture.h>
#include "upload-check.h"
#include <internals.h>

#include <assets/resources/decoded-image.h>
#include <glad/gl.h>
#include <memory>
#include <utility>

namespace CE::Assets {
    namespace {
        RenderAPIs::OpenGLHandle create_texture_handle(
            std::shared_ptr<RenderAPIs::OpenGLResourceLifetime> lifetime
        ) {
            if (!lifetime)
                throw Exceptions::invalid_args(CE_HERE, "OpenGL texture needs a resource lifetime");
            lifetime->require_current();
            RenderAPIs::require_no_gl_error("Cannot create a texture with pending OpenGL errors");
            GLuint id = 0;
            glGenTextures(1, &id);
            try {
                RenderAPIs::require_no_gl_error("OpenGL texture creation failed");
                return {lifetime, RenderAPIs::GLResourceKind::Texture, id};
            } catch (...) {
                lifetime->discard_untracked(RenderAPIs::GLResourceKind::Texture, id);
                throw;
            }
        }

        std::uint32_t texture_unit_limit() {
            GLint maximum_units = 0;
            glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maximum_units);
            RenderAPIs::require_no_gl_error("OpenGL texture binding-limit query failed");
            if (maximum_units <= 0)
                throw Exceptions::failed_operation(CE_HERE, "Current context reports no texture binding units");
            return static_cast<std::uint32_t>(maximum_units);
        }
    }

    template <typename T>
    void upload(
        const T* bits,
        int width,
        int height,
        bool use_mipmaps,
        bool pixelate,
        GLint wrap_opt,
        GLenum fmt
    ) {
        // Apply anisotropic filtering only for supported color textures; the red-only
        // font atlas uses swizzle and unpack-alignment handling below.
        if (fmt != GL_RED && GLAD_GL_EXT_texture_filter_anisotropic) {
            GLfloat largest_supported_anisotropy = 0;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &largest_supported_anisotropy);
            RenderAPIs::require_no_gl_error("OpenGL anisotropy-limit query failed");
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, largest_supported_anisotropy);
        }

        // Set sampling and wrap policy before uploading pixels; atlas textures
        // disable mipmaps while sprite textures may generate them afterward.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_opt);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_opt);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, use_mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, pixelate ? GL_NEAREST : GL_LINEAR);
        RenderAPIs::require_no_gl_error("OpenGL texture sampling configuration failed");

        const GLint internal_format = fmt == GL_RED ? GL_R8 : GL_RGBA8;
        GLint previous_unpack_alignment = 4;
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &previous_unpack_alignment);
        RenderAPIs::require_no_gl_error("OpenGL unpack-alignment query failed");
        if (fmt == GL_RED) {
            // One-byte atlas rows can be unaligned; map red to alpha while emitting white RGB.
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            constexpr GLint swizzle[]{GL_ONE, GL_ONE, GL_ONE, GL_RED};
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);
        }
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, fmt, GL_UNSIGNED_BYTE, bits);
        glPixelStorei(GL_UNPACK_ALIGNMENT, previous_unpack_alignment);
        RenderAPIs::require_no_gl_error("OpenGL texture storage upload failed");

        // Mip levels depend on the base image uploaded above.
        if (use_mipmaps) {
            glGenerateMipmap(GL_TEXTURE_2D);
            RenderAPIs::require_no_gl_error("OpenGL texture mipmap generation failed");
        }
    }

    Texture::Texture(
        std::shared_ptr<RenderAPIs::OpenGLResourceLifetime> lifetime,
        const char* file,
        bool use_mipmaps,
        bool pixelate,
        int wrap_opt
    ) {
        if (!file)
            throw Exceptions::invalid_args(CE_HERE, "Image filename must not be null");
        const auto pixels = decode_image(file);
        width = static_cast<int>(pixels.size.width);
        height = static_cast<int>(pixels.size.height);
        handle_ = create_texture_handle(std::move(lifetime));
        binding_unit_limit_ = texture_unit_limit();
        bind(0);
        upload(pixels.rgba.data(), width, height, use_mipmaps, pixelate, wrap_opt, GL_RGBA);
        unbind(0);
    }

    Texture::Texture(
        std::shared_ptr<RenderAPIs::OpenGLResourceLifetime> lifetime,
        const unsigned char* bitmap_data,
        int width,
        int height,
        bool use_mipmaps,
        bool pixelate,
        GLint wrap_opt,
        GLenum fmt
    )
    : width(width), height(height) {
        if (!bitmap_data || width <= 0 || height <= 0)
            throw Exceptions::invalid_args(CE_HERE, "Texture pixels and dimensions must be nonempty");
        // The font path supplies an already baked alpha atlas; upload() applies
        // its one-channel swizzle without running a file decoder.
        handle_ = create_texture_handle(std::move(lifetime));
        binding_unit_limit_ = texture_unit_limit();
        bind(0);
        upload(bitmap_data, width, height, use_mipmaps, pixelate, wrap_opt, fmt);
        unbind(0);
    }

    void Texture::require_binding(
        const std::uint32_t unit
    ) const {
        (void)handle_.id();
        if (unit >= binding_unit_limit_)
            throw Exceptions::invalid_args(CE_HERE, "Texture binding unit exceeds the current context's limit");
    }

    void Texture::bind(
        const std::uint32_t unit
    ) const {
        require_binding(unit);
        const auto id = handle_.id();
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, id);
    }

    void Texture::unbind(
        const std::uint32_t unit
    ) const {
        require_binding(unit);
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}
