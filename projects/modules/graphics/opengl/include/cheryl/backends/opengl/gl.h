#pragma once

namespace CE::Rendering::OpenGL {
    // Driver token values and object names carry no loader implementation or native ownership.
    using NativeHandle = unsigned int;
    using NativeEnum = unsigned int;
    using NativeInteger = int;
    namespace TextureWrap {
        inline constexpr int ClampToEdge = 0x812F;
        inline constexpr int Repeat = 0x2901;
        inline constexpr int MirroredRepeat = 0x8370;
    }
    namespace TextureFormat {
        inline constexpr NativeEnum Red = 0x1903;
        inline constexpr NativeEnum RGBA = 0x1908;
    }
}
