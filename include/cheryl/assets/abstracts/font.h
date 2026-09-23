#pragma once
#ifndef CEFONT_H
#define CEFONT_H
#include "asset2d.h"

#include <string>
#include <tuple>

namespace CE::Assets {
    struct FontDrawInfo : DrawInfo {
        float angle = 0.f;
    };
    using FontResources = std::tuple<std::shared_ptr<Geometry2D>, std::shared_ptr<Image>>;
    struct Font : protected Asset2D {
        explicit Font(const FontResources& data) : Asset2D(std::get<0>(data), std::get<1>(data)) {}
        ~Font() override = default;
        virtual void print(std::string text, FontDrawInfo* format) = 0;
    };
}
#endif
