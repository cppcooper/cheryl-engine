#include <text/font-collection.h>

#include "font-internal.h"
#include "font-fallback-internal.h"
#include <core/resources/fileio/fonts-system.h>
#include <internals/exceptions.h>

#include FT_TRUETYPE_TABLES_H

#include <algorithm>
#include <array>
#include <compare>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <map>
#include <new>
#include <string_view>
#include <utility>

namespace CE::Text {
    namespace {
        using namespace Detail;
        constexpr std::array<std::string_view, 6> automatic_families{
            "Arial", "Segoe UI", "Helvetica", "Noto Sans", "DejaVu Sans", "Liberation Sans"};
        constexpr int regular_font_weight = 400;
        constexpr int maximum_automatic_font_weight = 500; // Medium; exclude Semibold and heavier defaults.

        std::string family_key(const std::string_view name) {
            std::string result(name);
            for (auto& character : result) {
                if (character >= 'A' && character <= 'Z')
                    character = static_cast<char>(character - 'A' + 'a');
            }
            return result;
        }

        FontBytes read_font(const std::filesystem::path& path) {
            std::ifstream input(path, std::ios::binary | std::ios::ate);
            if (!input)
                throw Exceptions::runtime_exception(CE_HERE, "Unable to open font '" + path.string() + "'");
            const auto length = static_cast<std::streamoff>(input.tellg());
            if (length <= 0 || static_cast<std::uintmax_t>(length) > static_cast<std::uintmax_t>(std::numeric_limits<FT_Long>::max()) ||
                static_cast<std::uintmax_t>(length) > std::numeric_limits<std::size_t>::max() ||
                length > std::numeric_limits<std::streamsize>::max())
                throw Exceptions::runtime_exception(CE_HERE, "Invalid font size for '" + path.string() + "'");
            auto bytes = std::make_shared<std::vector<unsigned char>>(static_cast<std::size_t>(length));
            input.seekg(0);
            if (!input.read(reinterpret_cast<char*>(bytes->data()), static_cast<std::streamsize>(length)))
                throw Exceptions::runtime_exception(CE_HERE, "Unable to read font '" + path.string() + "'");
            return bytes;
        }

        FontFaceInfo face_info(FT_Face face, std::optional<std::filesystem::path> path, const std::uint32_t index) {
            if (face->num_glyphs <= 0 || static_cast<unsigned long>(face->num_glyphs) > std::numeric_limits<std::uint32_t>::max())
                throw Exceptions::runtime_exception(CE_HERE, "Font has an unsupported glyph count");
            return {face->family_name ? face->family_name : "", face->style_name ? face->style_name : "", std::move(path),
                index, static_cast<std::uint32_t>(face->num_glyphs), false};
        }

        struct StyleScore {
            bool heavy{};
            bool italic{};
            int weight_distance{};

            [[nodiscard]] auto operator<=>(const StyleScore&) const = default;
        };

        StyleScore style_score(FT_Face face) {
            const bool bold = (face->style_flags & FT_STYLE_FLAG_BOLD) != 0;
            int weight = bold ? 700 : regular_font_weight;
            if (const auto* table = static_cast<const TT_OS2*>(FT_Get_Sfnt_Table(face, FT_SFNT_OS2)); table && table->usWeightClass)
                weight = table->usWeightClass;
            // Black and Light faces can both lack the bold flag; rank their declared weights around Regular.
            return {bold || weight > maximum_automatic_font_weight, (face->style_flags & FT_STYLE_FLAG_ITALIC) != 0,
                std::abs(weight - regular_font_weight)};
        }

        struct Candidate {
            FontFaceData data;
            FontFaceInfo info;
            StyleScore style_score;
        };

        std::map<std::string, Candidate> discover_families(
            FT_Library library,
            const FontSelection& selection,
            const std::vector<std::string>& requested
        ) {
            std::map<std::string, Candidate> result;
            if (requested.empty())
                return result;
            const auto files = selection.system_directories ? Resources::find_system_fonts(*selection.system_directories)
                                                           : Resources::find_system_fonts();
            for (const auto& file : files) {
                FontBytes bytes;
                FreeTypeFace first(nullptr, FT_Done_Face);
                try {
                    bytes = read_font(file);
                    first = open_face(library, {bytes, 0});
                } catch (const Exceptions::runtime_exception&) {
                    continue; // Installed-font discovery is best effort; explicit files below are strict.
                }
                const auto count = first->num_faces;
                if (count <= 0 || static_cast<unsigned long>(count) > std::numeric_limits<std::uint32_t>::max())
                    continue;
                for (FT_Long index = 0; index < count; ++index) {
                    FreeTypeFace face(nullptr, FT_Done_Face);
                    try {
                        face = index == 0 ? std::move(first) : open_face(library, {bytes, static_cast<std::uint32_t>(index)});
                    } catch (const Exceptions::runtime_exception&) {
                        continue;
                    }
                    const auto key = family_key(face->family_name ? face->family_name : "");
                    if (std::ranges::find(requested, key) == requested.end())
                        continue;
                    const auto score = style_score(face.get());
                    const auto old = result.find(key);
                    if (old != result.end() && old->second.style_score <= score)
                        continue; // Equal styles keep sorted path/face order.
                    result.insert_or_assign(key, Candidate{{bytes, static_cast<std::uint32_t>(index)},
                        face_info(face.get(), file, static_cast<std::uint32_t>(index)), score});
                }
            }
            return result;
        }

        void append_face(FontCollectionData& result, const FontFaceData& data, const FontFaceInfo& info) {
            for (const auto& old : result.info) {
                if (old.path && info.path && *old.path == *info.path && old.face_index == info.face_index)
                    return;
            }
            result.faces.push_back(data);
            result.info.push_back(info);
        }
    }

    Detail::FreeTypeLibrary Detail::open_freetype() {
        FT_Library library = nullptr;
        const auto error = FT_Init_FreeType(&library);
        if (error == FT_Err_Out_Of_Memory)
            throw std::bad_alloc{};
        if (error)
            throw Exceptions::runtime_exception(CE_HERE, "Unable to initialize FreeType");
        return {library, FT_Done_FreeType};
    }

    Detail::FreeTypeFace Detail::open_face(FT_Library library, const FontFaceData& data, const std::uint32_t pixel_height) {
        if (!data.bytes || data.bytes->empty() || data.bytes->size() > static_cast<std::size_t>(std::numeric_limits<FT_Long>::max()) ||
            !std::in_range<FT_Long>(data.index))
            throw Exceptions::runtime_exception(CE_HERE, "Font bytes or face index exceed FreeType limits");
        FT_Face raw = nullptr;
        auto error = FT_New_Memory_Face(library, data.bytes->data(), static_cast<FT_Long>(data.bytes->size()), data.index, &raw);
        FreeTypeFace face(raw, FT_Done_Face);
        if (!error && data.index >= static_cast<unsigned long>(raw->num_faces))
            throw Exceptions::runtime_exception(CE_HERE, "Font face index must select a collection face, not a named variation");
        if (!error && (!FT_IS_SCALABLE(raw) || FT_Select_Charmap(raw, FT_ENCODING_UNICODE)))
            throw Exceptions::runtime_exception(CE_HERE, "A text font needs a scalable Unicode face");
        if (!error && pixel_height)
            error = FT_Set_Pixel_Sizes(raw, 0, pixel_height);
        if (error == FT_Err_Out_Of_Memory)
            throw std::bad_alloc{};
        if (error)
            throw Exceptions::runtime_exception(CE_HERE, "Unable to open or size a font face");
        return face;
    }

    const Detail::FontCollectionData& Detail::FontAccess::data(const FontCollection& collection) {
        if (!collection.data_)
            throw Exceptions::invalid_args(CE_HERE, "A moved-from font collection has no faces");
        return *collection.data_;
    }

    FontCollection::FontCollection(std::shared_ptr<const Detail::FontCollectionData> data)
    : data_(std::move(data)) {}

    FontCollection FontCollection::load(const FontSelection& selection) {
        auto library = Detail::open_freetype();
        std::vector<std::string> requested;
        for (const auto& source : selection.preferred) {
            if (const auto* family = std::get_if<SystemFontFamily>(&source)) {
                if (family->name.empty() || family->name.find('\0') != std::string::npos)
                    throw Exceptions::invalid_args(CE_HERE, "An installed font family needs a nonempty name without NUL");
                requested.push_back(family_key(family->name));
            }
        }
        if (selection.automatic_system_fonts) {
            for (const auto name : automatic_families)
                requested.push_back(family_key(name));
        }
        const auto installed = discover_families(library.get(), selection, requested);
        auto result = std::make_shared<Detail::FontCollectionData>();
        for (const auto& source : selection.preferred) {
            if (const auto* file = std::get_if<FontFile>(&source)) {
                const auto path = file->path.lexically_normal();
                const Detail::FontFaceData data{read_font(path), file->face_index};
                const auto face = Detail::open_face(library.get(), data);
                append_face(*result, data, face_info(face.get(), path, file->face_index));
            } else {
                const auto entry = installed.find(family_key(std::get<SystemFontFamily>(source).name));
                if (entry != installed.end())
                    append_face(*result, entry->second.data, entry->second.info);
            }
        }
        if (selection.automatic_system_fonts) {
            for (const auto name : automatic_families) {
                const auto entry = installed.find(family_key(name));
                if (entry != installed.end() && !entry->second.style_score.heavy)
                    append_face(*result, entry->second.data, entry->second.info);
            }
        }
        const auto embedded = Detail::builtin_font_bytes();
        const Detail::FontFaceData fallback{std::make_shared<const std::vector<unsigned char>>(embedded.begin(), embedded.end()), 0};
        const auto face = Detail::open_face(library.get(), fallback);
        if (!FT_Get_Char_Index(face.get(), U'\ufffd'))
            throw Exceptions::runtime_exception(CE_HERE, "The builtin font needs a visible replacement glyph");
        auto info = face_info(face.get(), std::nullopt, 0);
        info.builtin = true;
        append_face(*result, fallback, info);
        return FontCollection(std::move(result));
    }

    std::span<const FontFaceInfo> FontCollection::faces() const noexcept {
        return data_ ? std::span<const FontFaceInfo>{data_->info} : std::span<const FontFaceInfo>{};
    }

    bool FontCollection::covers(const char32_t scalar) const {
        if (scalar > 0x10ffff || (scalar >= 0xd800 && scalar <= 0xdfff))
            return false;
        const auto& data = Detail::FontAccess::data(*this);
        auto library = Detail::open_freetype();
        for (const auto& entry : data.faces) {
            const auto face = Detail::open_face(library.get(), entry);
            if (FT_Get_Char_Index(face.get(), scalar))
                return true;
        }
        return false;
    }
}
