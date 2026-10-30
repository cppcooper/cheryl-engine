#pragma once

#include <cheryl/export.h>

#include "image.h"

#include <filesystem>
#include <span>
#include <vector>

namespace CE::Assets::Images {
    /** Owned top-to-bottom RGBA8 pixels, independent of any graphics context.
     * Four bytes per pixel; aggregate construction does not validate byte count.
     * @see [Resource residency](cheryl-guide:resources/resource-residency)
     */
    struct Decoded {
        CE::Rendering::Geometry::PixelSize size; ///< Pixel width and height; uploads require nonzero dimensions.
        std::vector<unsigned char> rgba; ///< Owned interleaved red, green, blue and alpha bytes, row by row.
    };
    // CPU-only file decode; throws on unreadable/unsupported data or storage overflow.
    // Returns owned pixels without publishing a resource/cache entry.
    [[nodiscard]] CE_API Decoded decode(const std::filesystem::path& file);
    // CPU-only memory decode with the same owned RGBA representation as file decoding.
    [[nodiscard]] CE_API Decoded decode(std::span<const unsigned char> bytes);
}

/**
 * @fn CE::Assets::Images::Decoded CE::Assets::Images::decode(const std::filesystem::path& file)
 * @brief Decode a file into owned RGBA8 pixels without uploading or publishing a cache entry.
 * @param file Path opened during this call; relative paths use the working directory.
 * @return Top-to-bottom rows whose byte count matches the returned dimensions.
 * @throws CE::Exceptions::runtime_exception If the file cannot be decoded.
 * Allocation/storage-limit failures propagate; the input path is not retained.
 */

/**
 * @fn CE::Assets::Images::Decoded CE::Assets::Images::decode(std::span<const unsigned char> bytes)
 * @brief Decode an encoded image held in memory into the same RGBA8 representation.
 * @param bytes Encoded file bytes borrowed only through the call; this is not raw pixel data.
 * @return Independent owned pixels; no graphics context or provider is used.
 * Empty, oversized or undecodable input rejects the operation; allocation failures propagate.
 */
