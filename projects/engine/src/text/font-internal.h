#pragma once

#include <text/font-collection.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <memory>
#include <vector>

namespace CE::Text::Detail {
    using FontBytes = std::shared_ptr<const std::vector<unsigned char>>;
    struct FontFaceData {
        FontBytes bytes;
        std::uint32_t index{};
    };
    struct FontCollectionData {
        std::vector<FontFaceData> faces;
        std::vector<FontFaceInfo> info;
    };
    struct FontAccess {
        [[nodiscard]] static const FontCollectionData& data(const FontCollection& collection);
    };

    using FreeTypeLibrary = std::unique_ptr<FT_LibraryRec_, decltype(&FT_Done_FreeType)>;
    using FreeTypeFace = std::unique_ptr<FT_FaceRec_, decltype(&FT_Done_Face)>;

    [[nodiscard]] FreeTypeLibrary open_freetype();
    [[nodiscard]] FreeTypeFace open_face(FT_Library library, const FontFaceData& data, std::uint32_t pixel_height = 0);
}
