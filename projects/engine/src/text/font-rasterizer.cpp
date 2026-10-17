#include <text/font-rasterizer.h>

#include <internals/exceptions.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BITMAP_H
#include FT_GLYPH_H
#include FT_MULTIPLE_MASTERS_H
#include FT_OUTLINE_H
#include FT_STROKER_H
#include FT_TRUETYPE_TABLES_H

#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include <unordered_map>

namespace CE::Text {
    struct FontRasterizer::State {
        std::vector<unsigned char> bytes;
        FT_Library library{};
        FT_Face face{};
        mutable std::mutex mutex;
        mutable std::unordered_map<std::uint64_t, FontRasterMetrics> metrics_cache;

        ~State() {
            if (face)
                FT_Done_Face(face);
            if (library)
                FT_Done_FreeType(library);
        }

        std::optional<float> set_size(const FontRasterSize size) const {
            if (size.height == 0 || size.height > static_cast<std::uint32_t>(std::numeric_limits<int>::max() >> 6))
                return {};
            if (size.sizing == FontSizing::Pixels)
                return FT_Set_Pixel_Sizes(face, 0, size.height) == 0 ? std::optional{1.f} : std::nullopt;
            if (FT_Set_Char_Size(face, 0, static_cast<FT_F26Dot6>(size.height) << 6, 0, 0) == 0)
                return 1.f;
            int selected = -1;
            int best = std::numeric_limits<int>::max();
            for (int i = 0; i < face->num_fixed_sizes; ++i) {
                const auto difference = static_cast<int>(face->available_sizes[i].height) - static_cast<int>(size.height);
                const auto score = difference < 0 ? std::numeric_limits<int>::max() / 2 - difference : difference;
                if (score < best) {
                    selected = i;
                    best = score;
                }
            }
            if (selected < 0 || face->available_sizes[selected].height <= 0 || FT_Select_Size(face, selected) != 0)
                return {};
            const float scale = static_cast<float>(size.height) / face->available_sizes[selected].height;
            return scale >= .95f ? 1.f : scale;
        }
    };

    namespace {
        bool scalar_value(const char32_t scalar) {
            return scalar <= 0x10ffff && (scalar < 0xd800 || scalar > 0xdfff);
        }

        struct GlyphOwner {
            FT_Glyph value{};
            ~GlyphOwner() {
                if (value)
                    FT_Done_Glyph(value);
            }
        };

        std::vector<unsigned char> bitmap_pixels(const FT_Bitmap& bitmap, const bool color) {
            const std::size_t channels = color ? 4 : 1;
            const auto width = static_cast<std::size_t>(bitmap.width);
            const auto height = static_cast<std::size_t>(bitmap.rows);
            if (height != 0 && width > std::numeric_limits<std::size_t>::max() / channels / height)
                throw Exceptions::invalid_args(CE_HERE, "Font bitmap exceeds addressable storage");
            std::vector<unsigned char> pixels(width * height * channels);
            for (std::size_t y = 0; y < height; ++y) {
                const auto* row = bitmap.buffer + static_cast<std::ptrdiff_t>(y) * bitmap.pitch;
                for (std::size_t x = 0; x < width; ++x) {
                    auto* output = pixels.data() + (y * width + x) * channels;
                    if (bitmap.pixel_mode == FT_PIXEL_MODE_MONO)
                        output[0] = row[x / 8] & (0x80 >> (x % 8)) ? 255 : 0;
                    else if (color) {
                        output[0] = row[x * 4 + 2];
                        output[1] = row[x * 4 + 1];
                        output[2] = row[x * 4];
                        output[3] = row[x * 4 + 3];
                    } else
                        output[0] = row[x];
                }
            }
            return pixels;
        }
    }

    FontRasterizer::FontRasterizer(std::shared_ptr<State> state)
    : state_(std::move(state)) {}

    FontRasterizer FontRasterizer::load(
        const std::span<const unsigned char> bytes,
        const std::uint32_t face_index,
        const std::uint32_t named_instance,
        const bool unicode_only
    ) {
        if (bytes.empty() || bytes.size() > static_cast<std::size_t>(std::numeric_limits<FT_Long>::max()) || face_index > 0xffff ||
            named_instance > 0x7fff)
            throw Exceptions::invalid_args(CE_HERE, "Font bytes or face index are invalid");
        auto state = std::make_shared<State>();
        state->bytes.assign(bytes.begin(), bytes.end());
        if (FT_Init_FreeType(&state->library) != 0 || FT_New_Memory_Face(
                                                          state->library, state->bytes.data(), static_cast<FT_Long>(state->bytes.size()),
                                                          static_cast<FT_Long>((named_instance << 16) | face_index), &state->face
                                                      ) != 0)
            throw Exceptions::failed_operation(CE_HERE, "Could not open font face");
        if (unicode_only) {
            if (FT_Select_Charmap(state->face, FT_ENCODING_UNICODE) != 0)
                throw Exceptions::failed_operation(CE_HERE, "Font has no Unicode character map");
        } else if (!state->face->charmap && FT_Select_Charmap(state->face, FT_ENCODING_APPLE_ROMAN) != 0)
            throw Exceptions::failed_operation(CE_HERE, "Font has no supported character map");
        return FontRasterizer(std::move(state));
    }

    std::vector<FontVariation> FontRasterizer::variations(const std::span<const unsigned char> bytes, const std::uint32_t face_index) {
        const auto rasterizer = load(bytes, face_index);
        auto& state = *rasterizer.state_;
        FT_MM_Var* variation{};
        if (FT_Get_MM_Var(state.face, &variation) != 0)
            return {};
        const auto release = [&state](FT_MM_Var* value) { FT_Done_MM_Var(state.library, value); };
        const std::unique_ptr<FT_MM_Var, decltype(release)> owner(variation, release);
        unsigned int weight_axis = 0, width_axis = 1;
        for (unsigned int i = 0; i < variation->num_axis; ++i) {
            if (variation->axis[i].tag == 0x77676874)
                weight_axis = i;
            if (variation->axis[i].tag == 0x77647468)
                width_axis = i;
        }
        std::vector<FontVariation> result;
        if (variation->num_axis != 0) {
            result.reserve(variation->num_namedstyles);
            for (unsigned int i = 0; i < variation->num_namedstyles; ++i) {
                const auto coordinate = [&](const unsigned int axis, const std::uint16_t fallback) {
                    if (axis >= variation->num_axis)
                        return fallback;
                    const auto value = static_cast<std::uint16_t>(variation->namedstyle[i].coords[axis] / 0x10000);
                    return value == 0 ? fallback : value;
                };
                result.push_back({coordinate(weight_axis, 400), coordinate(width_axis, 100), i + 1});
            }
        }
        std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
            return left.weight == right.weight ? left.width < right.width : left.weight < right.weight;
        });
        return result;
    }

    FontRasterProperties FontRasterizer::properties() const {
        if (!state_)
            return {};
        const std::lock_guard lock(state_->mutex);
        const auto face = state_->face;
        const auto* os2 = static_cast<const TT_OS2*>(FT_Get_Sfnt_Table(face, FT_SFNT_OS2));
        const auto weight = os2 && os2->usWeightClass ? os2->usWeightClass : (face->style_flags & FT_STYLE_FLAG_BOLD ? 700 : 400);
        return {face->family_name ? face->family_name : "", face->style_name ? face->style_name : "",
                static_cast<std::uint16_t>(weight),         static_cast<bool>(face->style_flags & FT_STYLE_FLAG_ITALIC),
                static_cast<bool>(FT_IS_SCALABLE(face)),    static_cast<bool>(FT_HAS_KERNING(face))};
    }

    bool FontRasterizer::covers(const char32_t scalar) const {
        if (!state_ || !scalar_value(scalar))
            return false;
        const std::lock_guard lock(state_->mutex);
        return FT_Get_Char_Index(state_->face, static_cast<FT_ULong>(scalar)) != 0;
    }

    std::optional<FontRasterMetrics> FontRasterizer::metrics(const FontRasterSize size) const {
        if (!state_)
            return {};
        const std::lock_guard lock(state_->mutex);
        const auto key = (static_cast<std::uint64_t>(size.height) << 1) | (size.sizing == FontSizing::Characters ? 1 : 0);
        if (const auto found = state_->metrics_cache.find(key); found != state_->metrics_cache.end())
            return found->second;
        const auto scale = state_->set_size(size);
        if (!scale)
            return {};
        const auto face = state_->face;
        const auto factor = *scale / 64.f;
        const auto x_index = FT_Get_Char_Index(face, 'x');
        const auto x_height = x_index && FT_Load_Glyph(face, x_index, FT_LOAD_DEFAULT) == 0 ? face->glyph->metrics.height * factor
                                                                                            : face->size->metrics.height * factor * .5f;
        const FontRasterMetrics result{
            face->size->metrics.ascender * factor,
            face->size->metrics.descender * factor,
            face->size->metrics.height * factor,
            FT_MulFix(face->ascender, face->size->metrics.y_scale) * factor,
            FT_MulFix(face->descender, face->size->metrics.y_scale) * factor,
            FT_MulFix(face->ascender - face->descender, face->size->metrics.y_scale) * factor,
            FT_MulFix(-face->underline_position, face->size->metrics.y_scale) * factor,
            FT_MulFix(face->underline_thickness, face->size->metrics.y_scale) * factor,
            x_height,
            *scale,
            FT_Get_Char_Index(face, 0x2026) != 0
        };
        state_->metrics_cache.emplace(key, result);
        return result;
    }

    std::optional<FontRasterGlyph>
    FontRasterizer::rasterize(const char32_t scalar, const FontRasterSize size, const FontRasterOptions options) const {
        if (!state_ || !scalar_value(scalar) || !std::isfinite(options.outline) || options.outline < 0 ||
            options.outline > static_cast<float>(std::numeric_limits<FT_Fixed>::max() / 64))
            return {};
        const std::lock_guard lock(state_->mutex);
        const auto scale = state_->set_size(size);
        if (!scale)
            return {};
        const auto face = state_->face;
        FT_Int32 flags = FT_LOAD_DEFAULT;
        if (options.force_autohint)
            flags |= FT_LOAD_FORCE_AUTOHINT | FT_LOAD_TARGET_NORMAL;
        if (options.color)
            flags |= FT_LOAD_COLOR;
        if (options.outline != 0)
            flags |= FT_LOAD_NO_BITMAP;
        if (FT_Load_Char(face, static_cast<FT_ULong>(scalar), flags) != 0)
            return {};
        FontRasterGlyph result;
        result.advance = static_cast<float>(face->glyph->metrics.horiAdvance >> 6);
        result.left_hint_delta = static_cast<float>(face->glyph->lsb_delta) / 64;
        result.right_hint_delta = static_cast<float>(face->glyph->rsb_delta) / 64;
        result.bitmap_scale = *scale;
        GlyphOwner glyph;
        if (FT_Get_Glyph(face->glyph, &glyph.value) != 0)
            return {};
        if (options.outline != 0 && glyph.value->format == FT_GLYPH_FORMAT_OUTLINE) {
            FT_Stroker stroker{};
            if (FT_Stroker_New(state_->library, &stroker) != 0)
                return {};
            FT_Stroker_Set(stroker, static_cast<FT_Fixed>(options.outline * 64), FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);
            const auto error = FT_Glyph_Stroke(&glyph.value, stroker, true);
            FT_Stroker_Done(stroker);
            if (error != 0)
                return {};
        }
        const bool outline = glyph.value->format == FT_GLYPH_FORMAT_OUTLINE;
        if (options.bold && outline && FT_Outline_Embolden(&reinterpret_cast<FT_OutlineGlyph>(glyph.value)->outline, 64) != 0)
            return {};
        if (FT_Glyph_To_Bitmap(&glyph.value, FT_RENDER_MODE_NORMAL, nullptr, true) != 0)
            return {};
        const auto bitmap_glyph = reinterpret_cast<FT_BitmapGlyph>(glyph.value);
        auto& bitmap = bitmap_glyph->bitmap;
        if (options.bold && !outline && FT_Bitmap_Embolden(state_->library, &bitmap, 64, 64) != 0)
            return {};
        if (options.force_autohint)
            result.advance = static_cast<float>(bitmap_glyph->root.advance.x >> 16);
        if (options.bold)
            result.advance += 1;
        result.width = bitmap.width;
        result.height = bitmap.rows;
        result.left = bitmap_glyph->left;
        result.top = bitmap_glyph->top;
        const bool color = bitmap.pixel_mode == FT_PIXEL_MODE_BGRA;
        if (bitmap.pixel_mode != FT_PIXEL_MODE_MONO && bitmap.pixel_mode != FT_PIXEL_MODE_GRAY && !color)
            return {};
        result.format = color ? FontBitmapFormat::PremultipliedRGBA : FontBitmapFormat::Alpha;
        result.pixels = bitmap_pixels(bitmap, color);
        return result;
    }

    float FontRasterizer::kerning(const char32_t left, const char32_t right, const FontRasterSize size, const bool unfitted) const {
        if (!state_ || !scalar_value(left) || !scalar_value(right))
            return 0;
        const std::lock_guard lock(state_->mutex);
        if (!state_->set_size(size) || !FT_HAS_KERNING(state_->face))
            return 0;
        FT_Vector result{};
        if (FT_Get_Kerning(
                state_->face, FT_Get_Char_Index(state_->face, static_cast<FT_ULong>(left)),
                FT_Get_Char_Index(state_->face, static_cast<FT_ULong>(right)), unfitted ? FT_KERNING_UNFITTED : FT_KERNING_DEFAULT, &result
            ) != 0)
            return 0;
        return static_cast<float>(result.x) / 64;
    }
}
