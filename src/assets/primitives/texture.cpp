#include <assets/primitives/texture.h>
#include <resources/assets/texture-mgr.h>
#include <internals.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <glad/gl.h>

namespace CE::Assets {
    template<typename T>
    void upload(T* bits, int width, int height, GLuint slot,
        bool use_mipmaps, bool pixelate, GLint wrap_opt, GLenum fmt) {

        //the following turns on a special, high-quality filtering mode called "ANISOTROPY"
        if (fmt != GL_RED && GLAD_GL_EXT_texture_filter_anisotropic) {
            GLfloat largest_supported_anisotropy;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &largest_supported_anisotropy);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, largest_supported_anisotropy);
        }

        // Specify texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_opt);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_opt);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, use_mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, pixelate ? GL_NEAREST : GL_LINEAR);

        // Upload the bitmap data to the GPU (font bitmap is grayscale, so we use GL_RED)
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, width, height,
            0, fmt, GL_UNSIGNED_BYTE, bits);

        // Generate mipmaps if requested
        if (use_mipmaps) {
            glGenerateMipmap(GL_TEXTURE_2D);
        }
    }

    Texture::Texture(const char* file, int slot, bool use_mipmaps, bool pixelate, int wrap_opt)
    : unit(slot) {
        //image width and height, and #of components (1= gray scale, 4 = rgba)
        int channels(0);

        //retrieve the image data, currently force to RGBA (4 channels)
        stbi_uc* bits = stbi_load(file,
            &width, &height, &channels, 4);
        if (bits == nullptr || width <= 0 || height <= 0) {
            CELog::error("ERROR loading file: {}", file);
            if (bits == nullptr) CELog::critical("bits is nullptr");
            stbi_image_free(bits);
            if (width <= 0) CELog::error("width must be a positive non-zero integer");
            if (height <= 0) CELog::error("height must be a positive non-zero integer");
            throw Exceptions::runtime_exception(CE_HERE, "A problem was encountered when loading an image from disk.");
        }

        //generate an OpenGL texture ID for this texture
        glGenTextures(1, &id);
        bind();
        upload(bits, width, height, unit, use_mipmaps, pixelate, wrap_opt,GL_RGBA);
        //Free stb's copy of the data
        stbi_image_free(bits);
        unbind();
    }

    Texture::Texture(unsigned char* bitmap_data, int width, int height, GLuint slot,
        bool use_mipmaps, bool pixelate, GLint wrap_opt, GLenum fmt)
    : width(width), height(height), unit(slot) {

        // Generate and bind the texture
        glGenTextures(1, &id);
        bind();
        upload(bitmap_data, width, height, unit, use_mipmaps, pixelate, wrap_opt, fmt);
        unbind();
    }

    // Texture::Texture(unsigned char *bitmap, int width, int height, int slot,
    //     bool mipmaps, bool pixelate, int wrap_opt, int fmt) {
    //
    //     // Generate and bind the texture
    //     glGenTextures(1, &id);
    //     bind();
    //     upload(bitmap, width, height, slot, mipmaps, pixelate, wrap_opt, fmt);
    //     unbind();
    // }


    void Texture::bind() const {
        glActiveTexture(unit);
        glBindTexture(GL_TEXTURE_2D, id);
    }

    void Texture::unbind() {
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}
