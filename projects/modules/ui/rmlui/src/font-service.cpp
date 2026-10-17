/*
MIT License

Copyright (c) 2008-2014 CodePoint Ltd, Shift Technology Ltd, and contributors
Copyright (c) 2019-2026 The RmlUi Team, and contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
// Cheryl adaptation of RmlUi font bitmap conversion and downscaling through Engine.

#include <cheryl/text/font-rasterizer.h>

#include "font.h"
#include "../private/rasterizer-bridge.h"
#include <RmlUi/Core/Log.h>
#include <RmlUi/Core/Math.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <new>

namespace CE::UI::RmlUi::Detail {
    using namespace Rml;
    namespace {
        struct Face {
            CE::Text::FontRasterizer rasterizer;
            int size{};
        };

        Face& face(const FontFaceHandleFreetype handle) {
            return *reinterpret_cast<Face*>(handle);
        }

        CE::Text::FontRasterSize size(const int height) {
            return {height > 0 ? static_cast<std::uint32_t>(height) : 0, CE::Text::FontSizing::Characters};
        }

        void downscale(
            byte* destination,
            const int output_width,
            const int output_height,
            const byte* source,
            const int width,
            const int height,
            const int channels
        ) {
            const float x_scale = static_cast<float>(output_width) / width;
            const float y_scale = static_cast<float>(output_height) / height;
            const float sum_scale = x_scale * y_scale;
            float y_end = 0;
            for (int y_out = 0; y_out < output_height; ++y_out) {
                const auto y_start = y_end;
                y_end = std::min((y_out + 1) / y_scale, height - .001f);
                float x_end = 0;
                for (int x_out = 0; x_out < output_width; ++x_out) {
                    const auto x_start = x_end;
                    x_end = std::min((x_out + 1) / x_scale, width - .001f);
                    float sum[4]{};
                    for (int y = static_cast<int>(y_start); y <= static_cast<int>(y_end); ++y) {
                        auto y_part = 1.f;
                        if (y == static_cast<int>(y_start))
                            y_part -= y_start - y;
                        if (y == static_cast<int>(y_end))
                            y_part -= y + 1 - y_end;
                        for (int x = static_cast<int>(x_start); x <= static_cast<int>(x_end); ++x) {
                            auto x_part = 1.f;
                            if (x == static_cast<int>(x_start))
                                x_part -= x_start - x;
                            if (x == static_cast<int>(x_end))
                                x_part -= x + 1 - x_end;
                            for (int channel = 0; channel < channels; ++channel)
                                sum[channel] += source[(static_cast<std::size_t>(y) * width + x) * channels + channel] * y_part * x_part;
                        }
                    }
                    for (int channel = 0; channel < channels; ++channel)
                        destination[(static_cast<std::size_t>(y_out) * output_width + x_out) * channels + channel] =
                            static_cast<byte>(std::min(sum[channel] * sum_scale, 255.f));
                }
            }
        }

        bool append(Face& owner, const int font_size, const Character character, FontGlyphMap& glyphs) {
            if (!owner.rasterizer.covers(static_cast<char32_t>(character)))
                return false;
            const auto raster = owner.rasterizer.rasterize(static_cast<char32_t>(character), size(font_size), {.color = true});
            if (!raster || raster->width > static_cast<unsigned int>(std::numeric_limits<int>::max()) ||
                raster->height > static_cast<unsigned int>(std::numeric_limits<int>::max()))
                return false;
            FontGlyph glyph;
            glyph.bearing = {raster->left, raster->top};
            glyph.advance = static_cast<int>(raster->advance);
            glyph.bitmap_dimensions = {static_cast<int>(raster->width), static_cast<int>(raster->height)};
            if (raster->bitmap_scale < 1) {
                glyph.bearing = Vector2i(Vector2f(glyph.bearing) * raster->bitmap_scale);
                glyph.advance = static_cast<int>(glyph.advance * raster->bitmap_scale);
                glyph.bitmap_dimensions = Vector2i(Vector2f(glyph.bitmap_dimensions) * raster->bitmap_scale);
            }
            glyph.color_format = raster->format == CE::Text::FontBitmapFormat::PremultipliedRGBA ? ColorFormat::RGBA8 : ColorFormat::A8;
            const auto channels = glyph.color_format == ColorFormat::RGBA8 ? 4 : 1;
            const auto count = static_cast<std::size_t>(glyph.bitmap_dimensions.x) * glyph.bitmap_dimensions.y * channels;
            if (count) {
                glyph.bitmap_owned_data.reset(new byte[count]);
                glyph.bitmap_data = glyph.bitmap_owned_data.get();
                if (raster->bitmap_scale < 1)
                    downscale(
                        glyph.bitmap_owned_data.get(), glyph.bitmap_dimensions.x, glyph.bitmap_dimensions.y, raster->pixels.data(),
                        static_cast<int>(raster->width), static_cast<int>(raster->height), channels
                    );
                else
                    std::copy(raster->pixels.begin(), raster->pixels.end(), glyph.bitmap_owned_data.get());
            }
            owner.size = font_size;
            return glyphs.emplace(character, std::move(glyph)).second;
        }
    }

    static bool GetFaceVariations(const Span<const byte> data, Vector<FaceVariation>& output, const int face_index) {
        if (face_index < 0)
            return false;
        try {
            for (const auto& variation : CE::Text::FontRasterizer::variations({data.data(), data.size()}, face_index))
                output.push_back(
                    {static_cast<Style::FontWeight>(variation.weight), variation.width, static_cast<int>(variation.named_instance)}
                );
            return true;
        } catch (const std::bad_alloc&) {
            throw;
        } catch (const std::exception&) {
            return false;
        }
    }

    static FontFaceHandleFreetype LoadFace(const Span<const byte> data, const String& source, const int face_index, const int named_instance) {
        if (face_index < 0 || named_instance < 0)
            return 0;
        try {
            auto owner = std::make_unique<Face>();
            owner->rasterizer = CE::Text::FontRasterizer::load({data.data(), data.size()}, face_index, named_instance);
            return reinterpret_cast<FontFaceHandleFreetype>(owner.release());
        } catch (const std::bad_alloc&) {
            throw;
        } catch (const std::exception& error) {
            Log::Message(Log::LT_ERROR, "Engine could not load font from '%s': %s", source.c_str(), error.what());
            return 0;
        }
    }

    static bool ReleaseFace(const FontFaceHandleFreetype handle) {
        delete reinterpret_cast<Face*>(handle);
        return true;
    }

    static void GetFaceStyle(const FontFaceHandleFreetype handle, String* family, Style::FontStyle* style, Style::FontWeight* weight) {
        const auto properties = face(handle).rasterizer.properties();
        if (family)
            *family = properties.family;
        if (style)
            *style = properties.italic ? Style::FontStyle::Italic : Style::FontStyle::Normal;
        if (weight)
            *weight = static_cast<Style::FontWeight>(properties.weight);
    }

    static bool InitialiseFaceHandle(
        const FontFaceHandleFreetype handle,
        const int font_size,
        FontGlyphMap& glyphs,
        FontMetrics& metrics,
        const bool load_default_glyphs
    ) {
        auto& owner = face(handle);
        const auto value = owner.rasterizer.metrics(size(font_size));
        if (!value)
            return false;
        owner.size = font_size;
        metrics = {
            font_size,
            value->ascent,
            -value->descent,
            value->line_spacing,
            value->x_height,
            value->underline_position,
            std::max(value->underline_thickness, 1.f),
            value->has_ellipsis
        };
        if (load_default_glyphs) {
            glyphs.reserve(128);
            for (unsigned int character = 32; character <= 126; ++character)
                static_cast<void>(append(owner, font_size, static_cast<Character>(character), glyphs));
        }
        if (!glyphs.contains(Character::Replacement)) {
            FontGlyph replacement;
            replacement.bitmap_dimensions = {font_size / 3, font_size * 2 / 3};
            replacement.advance = replacement.bitmap_dimensions.x + 2;
            replacement.bearing = {1, replacement.bitmap_dimensions.y};
            const auto count = static_cast<std::size_t>(replacement.bitmap_dimensions.x) * replacement.bitmap_dimensions.y;
            replacement.bitmap_owned_data.reset(new byte[count]);
            replacement.bitmap_data = replacement.bitmap_owned_data.get();
            for (int y = 0; y < replacement.bitmap_dimensions.y; ++y)
                for (int x = 0; x < replacement.bitmap_dimensions.x; ++x)
                    replacement.bitmap_owned_data[static_cast<std::size_t>(y) * replacement.bitmap_dimensions.x + x] =
                        x == 0 || y == 0 || x + 1 == replacement.bitmap_dimensions.x || y + 1 == replacement.bitmap_dimensions.y ? 0xdd : 0;
            glyphs.emplace(Character::Replacement, std::move(replacement));
        }
        return true;
    }

    static bool AppendGlyph(const FontFaceHandleFreetype handle, const int font_size, const Character character, FontGlyphMap& glyphs) {
        return append(face(handle), font_size, character, glyphs);
    }

    static int GetKerning(const FontFaceHandleFreetype handle, const int font_size, const Character left, const Character right) {
        auto& owner = face(handle);
        if (font_size > 0)
            owner.size = font_size;
        const auto raster_size = size(owner.size);
        const auto metrics = owner.rasterizer.metrics(raster_size);
        if (!metrics || metrics->bitmap_scale != 1)
            return 0;
        return static_cast<int>(owner.rasterizer.kerning(static_cast<char32_t>(left), static_cast<char32_t>(right), raster_size));
    }

    static bool HasKerning(const FontFaceHandleFreetype handle) {
        return face(handle).rasterizer.properties().kerning;
    }

    const CherylRmlUiPrivate::RasterizerCallbacks& rasterizer_callbacks() {
        static const CherylRmlUiPrivate::RasterizerCallbacks callbacks{
            1,
            sizeof(CherylRmlUiPrivate::RasterizerCallbacks),
            GetFaceVariations,
            LoadFace,
            ReleaseFace,
            GetFaceStyle,
            InitialiseFaceHandle,
            AppendGlyph,
            GetKerning,
            HasKerning
        };
        return callbacks;
    }
}
