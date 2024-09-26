#pragma once
#ifndef CEFONT_H
#define CEFONT_H
#include <assets/primitives.h>

namespace CE::Assets {
    struct FontDrawInfo : DrawInfo {
        float angle = 0.f;
    };
    using VAOTex = std::tuple<std::shared_ptr<Vertex2D>, uint32_t, std::shared_ptr<Texture>>;
    struct Font : protected Asset2D {
        explicit Font(const VAOTex &data) :
        Asset2D(std::get<0>(data),std::get<1>(data),std::get<2>(data)) {}
        ~Font() override = default;
        virtual void print(std::string text, FontDrawInfo* format) = 0;
    };
}
#endif
