////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// TGUI - Texus' Graphical User Interface
// Copyright (C) 2012-2026 Bruno Van de Velde (vdv_b@tgui.eu)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Cheryl adaptation: Engine rasterization with TGUI glyph packing and texture generations.

#include "font.h"

#include <TGUI/Backend/Window/Backend.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace CE::UI::TGUI::Detail {
    CE::Text::FontRasterSize FontBackend::size(const unsigned int character_size) const {
        const auto scaled = character_size * m_fontScale;
        if (!std::isfinite(scaled) || scaled < 1 || static_cast<double>(scaled) > std::numeric_limits<unsigned int>::max())
            return {};
        return {static_cast<unsigned int>(scaled)};
    }

    std::optional<CE::Text::FontRasterMetrics> FontBackend::metrics(const unsigned int character_size) const {
        return face_.metrics(size(character_size));
    }

    bool FontBackend::loadFromMemory(std::unique_ptr<std::uint8_t[]> data, const std::size_t bytes) {
        CE::Text::FontRasterizer face;
        try {
            face = CE::Text::FontRasterizer::load({data.get(), bytes}, 0, 0, true);
        } catch (const std::bad_alloc&) {
            throw;
        } catch (const std::exception& error) {
            throw tgui::Exception{error.what()};
        }
        const auto scalable = face.properties().scalable;
        constexpr unsigned int initial_size = 128;
        std::vector<std::uint8_t> pixels(initial_size * initial_size * 4, 255);
        for (std::size_t i = 3; i < pixels.size(); i += 4)
            pixels[i] = 0;
        for (unsigned int y = 0; y < 2; ++y)
            for (unsigned int x = 0; x < 2; ++x)
                pixels[(y * initial_size + x) * 4 + 3] = 255;
        face_ = std::move(face);
        scalable_ = scalable;
        pixels_ = std::move(pixels);
        glyphs_.clear();
        rows_.clear();
        texture_.reset();
        texture_size_ = initial_size;
        texture_version_ = 0;
        next_row_ = 3;
        return true;
    }

    bool FontBackend::hasGlyph(const char32_t code) const {
        return face_.covers(code);
    }

    const FontBackend::Glyph&
    FontBackend::glyph(const char32_t code, const unsigned int character_size, const bool bold, const float outline) {
        const auto raster_size = size(character_size);
        const auto key = constructGlyphKey(code, raster_size.height, bold, outline * m_fontScale);
        if (const auto found = glyphs_.find(key); found != glyphs_.end())
            return found->second;
        Glyph value;
        const auto raster = face_.rasterize(code, raster_size, {true, bold, outline * m_fontScale});
        if (raster) {
            value.value.advance = raster->advance;
            value.value.bounds = {
                static_cast<float>(raster->left), static_cast<float>(-raster->top), static_cast<float>(raster->width),
                static_cast<float>(raster->height)
            };
            value.left_delta = raster->left_hint_delta;
            value.right_delta = raster->right_hint_delta;
            if (raster->width && raster->height) {
                constexpr unsigned int padding = 2;
                auto rect = reserve(raster->width + 2 * padding, raster->height + 2 * padding);
                rect.left += padding;
                rect.top += padding;
                rect.width -= 2 * padding;
                rect.height -= 2 * padding;
                value.value.textureRect = rect;
                for (unsigned int y = 0; y < raster->height; ++y)
                    for (unsigned int x = 0; x < raster->width; ++x)
                        pixels_[((rect.top + y) * texture_size_ + rect.left + x) * 4 + 3] =
                            raster->pixels[static_cast<std::size_t>(y) * raster->width + x];
                texture_.reset();
            }
        }
        return glyphs_.emplace(key, std::move(value)).first->second;
    }

    tgui::FontGlyph FontBackend::getGlyph(const char32_t code, const unsigned int character_size, const bool bold, const float outline) {
        auto result = glyph(code, character_size, bold, outline).value;
        result.advance /= m_fontScale;
        result.bounds.setPosition(result.bounds.getPosition() / m_fontScale);
        result.bounds.setSize(result.bounds.getSize() / m_fontScale);
        return result;
    }

    float FontBackend::getKerning(const char32_t left, const char32_t right, const unsigned int character_size, const bool bold) {
        if (!left || !right || !face_)
            return 0;
        const float first_delta = glyph(left, character_size, bold, 0).right_delta;
        const float second_delta = glyph(right, character_size, bold, 0).left_delta;
        const auto offset = face_.kerning(left, right, size(character_size), true);
        if (!scalable_)
            return offset * 64 / m_fontScale;
        return std::floor(second_delta - first_delta + offset + .5f) / m_fontScale;
    }

    float FontBackend::getLineSpacing(const unsigned int character_size) {
        const auto value = metrics(character_size);
        return value ? value->line_spacing / m_fontScale : 0;
    }

    float FontBackend::getFontHeight(const unsigned int character_size) {
        const auto value = metrics(character_size);
        if (!value)
            return 0;
        return (scalable_ ? std::ceil(value->design_height) : value->line_spacing) / m_fontScale;
    }

    float FontBackend::getAscent(const unsigned int character_size) {
        const auto value = metrics(character_size);
        return value ? (scalable_ ? std::ceil(value->design_ascent) : value->ascent) / m_fontScale : 0;
    }

    float FontBackend::getDescent(const unsigned int character_size) {
        const auto value = metrics(character_size);
        return value ? (scalable_ ? std::ceil(value->design_descent) : value->descent) / m_fontScale : 0;
    }

    float FontBackend::getUnderlinePosition(const unsigned int character_size) {
        const auto value = metrics(character_size);
        return value ? (scalable_ ? value->underline_position / m_fontScale : character_size / 10.f) : 0;
    }

    float FontBackend::getUnderlineThickness(const unsigned int character_size) {
        const auto value = metrics(character_size);
        return value ? (scalable_ ? value->underline_thickness / m_fontScale : character_size / 14.f) : 0;
    }

    std::shared_ptr<tgui::BackendTexture> FontBackend::getTexture(unsigned int, unsigned int& version) {
        if (!texture_) {
            auto texture = tgui::getBackend()->getRenderer()->createTexture();
            if (!texture->loadTextureOnly({texture_size_, texture_size_}, pixels_.data(), m_isSmooth))
                throw tgui::Exception{U"Could not publish font texture"};
            texture_ = std::move(texture);
            ++texture_version_;
        }
        version = texture_version_;
        return texture_;
    }

    tgui::Vector2u FontBackend::getTextureSize(unsigned int) {
        return {texture_size_, texture_size_};
    }

    void FontBackend::setSmooth(const bool smooth) {
        BackendFont::setSmooth(smooth);
        if (texture_)
            texture_->setSmooth(smooth);
    }

    void FontBackend::setFontScale(const float scale) {
        if (!std::isfinite(scale) || scale <= 0)
            throw tgui::Exception{U"Font scale must be finite and positive"};
        if (m_fontScale == scale)
            return;
        BackendFont::setFontScale(scale);
        texture_.reset();
    }

    tgui::UIntRect FontBackend::reserve(const unsigned int width, const unsigned int height) {
        Row* selected{};
        float best_ratio = 0;
        for (auto& row : rows_) {
            const auto ratio = static_cast<float>(height) / row.height;
            if (ratio < .7f || ratio > 1 || width > texture_size_ - row.width || ratio < best_ratio)
                continue;
            selected = &row;
            best_ratio = ratio;
        }
        if (!selected) {
            const auto row_height = height + height / 10;
            while (next_row_ + row_height >= texture_size_ || width >= texture_size_) {
                if (texture_size_ > std::numeric_limits<unsigned int>::max() / 2 ||
                    texture_size_ > std::numeric_limits<std::size_t>::max() / texture_size_ / 16)
                    throw tgui::Exception{U"Font atlas exceeds addressable storage"};
                const auto next_size = texture_size_ * 2;
                std::vector<std::uint8_t> pixels(static_cast<std::size_t>(next_size) * next_size * 4, 255);
                for (std::size_t i = 3; i < pixels.size(); i += 4)
                    pixels[i] = 0;
                for (unsigned int y = 0; y < texture_size_; ++y)
                    std::copy_n(
                        pixels_.begin() + static_cast<std::size_t>(y) * texture_size_ * 4, texture_size_ * 4,
                        pixels.begin() + static_cast<std::size_t>(y) * next_size * 4
                    );
                pixels_ = std::move(pixels);
                texture_size_ = next_size;
            }
            rows_.push_back({0, next_row_, row_height});
            next_row_ += row_height;
            selected = &rows_.back();
        }
        const tgui::UIntRect result{selected->width, selected->top, width, height};
        selected->width += width;
        return result;
    }
}
