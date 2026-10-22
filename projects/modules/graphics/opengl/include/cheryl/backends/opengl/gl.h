#pragma once

/** @file
 * @brief Loader-independent OpenGL object-name and token spellings; no native ownership.
 */

namespace CE::Rendering::OpenGL {
    // Driver token values and object names carry no loader implementation or native ownership.
    using NativeHandle = unsigned int; ///< OpenGL object name; carries no ownership or domain validation.
    using NativeEnum = unsigned int; ///< Native token value without a loader-header dependency.
    using NativeInteger = int; ///< Signed native count/location representation; -1 can mean an absent location.
    namespace TextureWrap {
        inline constexpr int ClampToEdge = 0x812F; ///< Native GL_CLAMP_TO_EDGE wrap token.
        inline constexpr int Repeat = 0x2901; ///< Native GL_REPEAT wrap token.
        inline constexpr int MirroredRepeat = 0x8370; ///< Native GL_MIRRORED_REPEAT wrap token.
    }
    namespace TextureFormat {
        inline constexpr NativeEnum Red = 0x1903; ///< One unsigned alpha byte per pixel on the font-atlas path.
        inline constexpr NativeEnum RGBA = 0x1908; ///< Four unsigned color/alpha bytes per pixel; input rows are top-to-bottom.
    }
}
