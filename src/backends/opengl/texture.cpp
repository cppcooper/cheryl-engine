#include <backends/opengl/texture.h>
#include <internals.h>

#include <assets/resources/decoded-image.h>
#include <glad/gl.h>
#include <memory>
#include <utility>

namespace CE::Assets {
    namespace {
        RenderAPIs::OpenGLHandle create_texture_handle(std::shared_ptr<RenderAPIs::OpenGLResourceLifetime> lifetime) {
            if (!lifetime)
                throw Exceptions::invalid_args(CE_HERE, "OpenGL texture needs a resource lifetime");
            lifetime->require_current();
            GLuint id = 0;
            glGenTextures(1, &id);
            try {
                return {std::move(lifetime), RenderAPIs::GLResourceKind::Texture, id};
            }
            catch (...) {
                if (id)
                    glDeleteTextures(1, &id);
                throw;
            }
        }
    }

    template <typename T>
    void upload(const T* bits, int width, int height, GLuint slot, bool use_mipmaps, bool pixelate, GLint wrap_opt, GLenum fmt) {

        // Apply anisotropic filtering only for supported color textures; the red-only
        // font atlas uses swizzle and unpack-alignment handling below.
        if (fmt != GL_RED && GLAD_GL_EXT_texture_filter_anisotropic) {
            GLfloat largest_supported_anisotropy;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &largest_supported_anisotropy);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, largest_supported_anisotropy);
        }

        // Set sampling and wrap policy before uploading pixels; atlas textures
        // disable mipmaps while sprite textures may generate them afterward.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_opt);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_opt);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, use_mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, pixelate ? GL_NEAREST : GL_LINEAR);

        const GLint internal_format = fmt == GL_RED ? GL_R8 : GL_RGBA8;
        GLint previous_unpack_alignment = 4;
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &previous_unpack_alignment);
        if (fmt == GL_RED) {
            // One-byte atlas rows can be unaligned; map red to alpha while emitting white RGB.
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            constexpr GLint swizzle[]{GL_ONE, GL_ONE, GL_ONE, GL_RED};
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);
        }
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, fmt, GL_UNSIGNED_BYTE, bits);
        glPixelStorei(GL_UNPACK_ALIGNMENT, previous_unpack_alignment);

        // Mip levels depend on the base image uploaded above.
        if (use_mipmaps) {
            glGenerateMipmap(GL_TEXTURE_2D);
        }
    }

    Texture::Texture(std::shared_ptr<RenderAPIs::OpenGLResourceLifetime> lifetime,
                     const char* file,
                     int slot,
                     bool use_mipmaps,
                     bool pixelate,
                     int wrap_opt)
        : unit(slot) {
        if (!file)
            throw Exceptions::invalid_args(CE_HERE, "Image filename must not be null");
        const auto pixels = decode_image(file);
        width = static_cast<int>(pixels.size.width);
        height = static_cast<int>(pixels.size.height);
        handle_ = create_texture_handle(std::move(lifetime));
        bind();
        upload(pixels.rgba.data(), width, height, unit, use_mipmaps, pixelate, wrap_opt, GL_RGBA);
        unbind();
    }

    Texture::Texture(std::shared_ptr<RenderAPIs::OpenGLResourceLifetime> lifetime,
                     const unsigned char* bitmap_data,
                     int width,
                     int height,
                     GLuint slot,
                     bool use_mipmaps,
                     bool pixelate,
                     GLint wrap_opt,
                     GLenum fmt)
        : width(width), height(height), unit(slot) {

        if (!bitmap_data || width <= 0 || height <= 0)
            throw Exceptions::invalid_args(CE_HERE, "Texture pixels and dimensions must be nonempty");
        // The font path supplies an already baked alpha atlas; upload() applies
        // its one-channel swizzle without running a file decoder.
        handle_ = create_texture_handle(std::move(lifetime));
        bind();
        upload(bitmap_data, width, height, unit, use_mipmaps, pixelate, wrap_opt, fmt);
        unbind();
    }

    void Texture::bind() const {
        const auto id = handle_.id();
        glActiveTexture(unit);
        glBindTexture(GL_TEXTURE_2D, id);
    }

    void Texture::unbind() { glBindTexture(GL_TEXTURE_2D, 0); }
}
