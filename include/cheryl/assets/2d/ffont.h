#pragma once
#ifndef FFONT_H
#define FFONT_H

#include <assets/abstracts.h>
#include <templates/singleton.h>

#include <filesystem>
#include <tuple>
#include <array>
#include <memory>

namespace CE::Assets {
    constexpr uint16_t num_chars_ffont = 256;
    using FFontData = std::tuple<std::array<float, num_chars_ffont>, std::shared_ptr<Vertex2D>, uint32_t, std::shared_ptr<Texture>>;

    struct FFontFormat : FontDrawInfo {
        bool fancy = false;
    };

    struct FFont final : Font, Singleton_CTS<FFont> {
        // call FFont::get(load_ffont(widths_file)) for construction
        explicit FFont(const FFontData &data) :
        Font({std::get<1>(data), std::get<2>(data), std::get<3>(data)}),
        widths(std::get<0>(data)) {}
        ~FFont() override = default;
        void print(std::string text, FontDrawInfo* format) override;
        static FFontData load_ffont(const std::filesystem::path& path);
    private:
        std::array<float, num_chars_ffont> widths;
        bool print_fancy = false;
        float print_angle = 0.f;
        std::string print_msg;
    protected:
        void draw(const DrawInfo& info) override;
    };
}
#endif

