#pragma once
#ifndef STBFONT_H
#define STBFONT_H

#include <assets/abstracts.h>

#include <glm.hpp>

#include <filesystem>
#include <tuple>
#include <memory>

namespace CE::Assets {
    using STBFontData = std::tuple<std::shared_ptr<Vertex2D>, std::size_t, std::shared_ptr<Texture>>;

    struct STBFont final : Font {
        explicit STBFont(const STBFontData &data) : Font(data) {}
        ~STBFont() override = default;
        void print(std::string text, FontDrawInfo *format) override;
        static STBFontData load_font(const char* font_path, int font_size);

    protected:
        void draw(const DrawInfo &info) override;

    private:
        float print_angle = 0.f;
        std::string print_msg;
    };
}
#endif //STBFONT_H
