#pragma once

#include <cheryl/export.h>
#ifndef TEXTURE_H
#define TEXTURE_H

#include <assets/resources/image.h>

#include <backends/opengl/gl.h>
#include <backends/opengl/resource-lifetime.h>
#include <cstdint>
#include <memory>

namespace CE::Rendering::OpenGL {
    /** Immutable uploaded 2D texture; public dimensions describe the upload and must not be edited to simulate resizing.
     * RGBA input rows are top-to-bottom and flipped during upload; red alpha atlases
     * keep the font baker's row order and sample as white RGB with supplied alpha.
     * Retained Handle retirement performs no native calls from destructors.
     * @see [Resource residency](cheryl-guide:resources/resource-residency)
     */
    struct CE_TYPE_API Texture final : CE::Rendering::Resources::Image {
    private:
        Handle handle_;
        std::uint32_t binding_unit_limit_ = 0;
        bool mipmap_complete_ = false;

    public:
        std::int32_t width{}; ///< Uploaded pixel width; changing metadata does not resize native storage.
        std::int32_t height{}; ///< Uploaded pixel height; retain its construction value.
        CE_OPENGL_API explicit Texture(std::shared_ptr<ResourceLifetime> lifetime, const char* file, bool use_mipmaps, bool pixelate, int wrap_opt);
        // Raw RGBA rows are top-to-bottom, matching CE::Assets::Images::Decoded. Red-only stb
        // atlas rows retain the order expected by stb's baked UV coordinates.
        CE_OPENGL_API explicit Texture(
            std::shared_ptr<ResourceLifetime> lifetime,
            const unsigned char* bitmap_data,
            int width,
            int height,
            bool use_mipmaps,
            bool pixelate,
            int wrap_opt = TextureWrap::ClampToEdge,
            NativeEnum fmt = TextureFormat::Red
        );
        [[nodiscard]] CE::Rendering::Geometry::PixelSize pixel_size() const override {
            return {width > 0 ? static_cast<std::uint32_t>(width) : 0, height > 0 ? static_cast<std::uint32_t>(height) : 0};
        }
        CE_OPENGL_API void bind(std::uint32_t unit) const override;
        CE_OPENGL_API void require_binding(std::uint32_t unit) const;
        [[nodiscard]] bool has_complete_mipmaps() const noexcept { return mipmap_complete_; }
        [[nodiscard]] const ResourceLifetime* resource_domain() const noexcept { return handle_.resource_domain(); }
        // Select the requested unit explicitly and require this texture's live context.
        CE_OPENGL_API void unbind(std::uint32_t unit) const;
    };
}

#endif

/**
 * @fn CE::Rendering::OpenGL::Texture::Texture(std::shared_ptr<ResourceLifetime> lifetime, const char* file, bool use_mipmaps, bool pixelate, int wrap_opt)
 * @brief Decode RGBA pixels and upload them in the supplied live native domain.
 * @param lifetime Required active owner/current-context lifetime.
 * @param file Nonnull NUL-terminated file path, borrowed through decoding.
 * @param use_mipmaps Generate the complete mip chain after uploading the base image.
 * @param pixelate Select nearest min/magnification and disable anisotropy; false selects linear filtering and available maximum anisotropy.
 * @param wrap_opt Valid native wrap token applied to both axes.
 * Decode/native/allocation failures propagate and adopted names retire normally. Upload uses and then unbinds texture unit 0; no cache entry is published.
 */

/**
 * @fn CE::Rendering::OpenGL::Texture::Texture(std::shared_ptr<ResourceLifetime> lifetime, const unsigned char* bitmap_data, int width, int height, bool use_mipmaps, bool pixelate, int wrap_opt, NativeEnum fmt)
 * @brief Copy raw pixels into immutable native storage on its live owner/current context.
 * @param lifetime Required active native domain.
 * @param bitmap_data Nonnull contiguous pixel bytes borrowed until return; caller supplies a complete addressable array.
 * @param width Positive pixel width.
 * @param height Positive pixel height.
 * @param use_mipmaps Generate mip levels; a 1 by 1 base image is already complete.
 * @param pixelate Nearest filtering without anisotropy when true.
 * @param wrap_opt Valid native wrap token for both axes.
 * @param fmt Red for one-byte alpha in baked row order, RGBA for top-to-bottom four-byte color.
 * Malformed dimensions/null data throw invalid_args; native/allocation failures propagate. Unit 0 is used then unbound; caller pixels are not retained.
 */

/**
 * @fn CE::Rendering::Geometry::PixelSize CE::Rendering::OpenGL::Texture::pixel_size() const
 * @return Copied public dimensions as nonnegative pixel sizes, without native calls. This reads metadata, not live native storage.
 */

/**
 * @fn void CE::Rendering::OpenGL::Texture::require_binding(std::uint32_t unit) const
 * @brief Validate live owner/current context and unit limit without changing bindings.
 * @param unit Zero-based unit below the construction-context limit.
 * Invalid units throw invalid_args; inactive/foreign/noncurrent lifetime throws failed_operation.
 */

/**
 * @fn void CE::Rendering::OpenGL::Texture::bind(std::uint32_t unit) const
 * @brief Select the validated unit, bind this texture and clear any sampler-object override there.
 * @param unit Zero-based supported unit.
 * Requires live owner/current context. Default texture sampling applies until a separate Sampler is bound; no material/program is selected.
 */

/**
 * @fn void CE::Rendering::OpenGL::Texture::unbind(std::uint32_t unit) const
 * @brief Select the validated unit and unbind both its texture and sampler object.
 * @param unit Zero-based supported unit.
 * Requires this texture's live owner/current context; does not destroy the image or restore prior native bindings.
 */

/**
 * @fn bool CE::Rendering::OpenGL::Texture::has_complete_mipmaps() const noexcept
 * @return Whether upload generated the full chain or the base image was 1 by 1; CPU metadata inspection only.
 */

/**
 * @fn const CE::Rendering::OpenGL::ResourceLifetime* CE::Rendering::OpenGL::Texture::resource_domain() const noexcept
 * @return Borrowed identity of the retained native lifetime; identity does not check current/active state.
 */
