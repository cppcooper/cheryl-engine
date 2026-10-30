#pragma once

#include <cheryl/text/font-rasterizer.h>
#include <cheryl/text/font.h>

#include <TGUI/Backend/Font/BackendFont.hpp>

#include <unordered_map>

namespace CE::UI::TGUI::Detail {
    class FontBackend final : public tgui::BackendFont {
        struct Glyph {
            tgui::FontGlyph value;
            float left_delta{};
            float right_delta{};
        };
        struct Row {
            unsigned int width{};
            unsigned int top{};
            unsigned int height{};
        };
        struct Face {
            CE::Text::FontRasterizer rasterizer;
            CE::Text::FontStyle style;
            std::size_t family_index{};
        };

        std::vector<Face> faces_;
        CE::Text::FontStyle style_;
        std::unordered_map<std::uint64_t, Glyph> glyphs_;
        std::vector<Row> rows_;
        std::vector<std::uint8_t> pixels_;
        std::shared_ptr<tgui::BackendTexture> texture_;
        unsigned int texture_size_{};
        unsigned int texture_version_{};
        unsigned int next_row_ = 3;
        bool scalable_{};

        [[nodiscard]] CE::Text::FontRasterSize size(unsigned int character_size) const;
        [[nodiscard]] const Face* primary(bool bold = false) const;
        [[nodiscard]] const Face* face(char32_t code, bool bold) const;
        void reset();
        [[nodiscard]] std::optional<CE::Text::FontRasterMetrics> metrics(unsigned int character_size) const;
        [[nodiscard]] const Glyph& glyph(char32_t code, unsigned int character_size, bool bold, float outline);
        [[nodiscard]] tgui::UIntRect reserve(unsigned int width, unsigned int height);

    public:
        using BackendFont::loadFromMemory;
        void load(const CE::Text::Font& font, CE::Text::FontStyle style);
        [[nodiscard]] bool loadFromMemory(std::unique_ptr<std::uint8_t[]> data, std::size_t bytes) override;
        [[nodiscard]] bool hasGlyph(char32_t code) const override;
        [[nodiscard]] tgui::FontGlyph getGlyph(char32_t code, unsigned int size, bool bold, float outline = 0) override;
        [[nodiscard]] float getKerning(char32_t left, char32_t right, unsigned int size, bool bold) override;
        [[nodiscard]] float getLineSpacing(unsigned int size) override;
        [[nodiscard]] float getFontHeight(unsigned int size) override;
        [[nodiscard]] float getAscent(unsigned int size) override;
        [[nodiscard]] float getDescent(unsigned int size) override;
        [[nodiscard]] float getUnderlinePosition(unsigned int size) override;
        [[nodiscard]] float getUnderlineThickness(unsigned int size) override;
        [[nodiscard]] std::shared_ptr<tgui::BackendTexture> getTexture(unsigned int size, unsigned int& version) override;
        [[nodiscard]] tgui::Vector2u getTextureSize(unsigned int size) override;
        void setSmooth(bool smooth) override;
        void setFontScale(float scale) override;
    };
}
