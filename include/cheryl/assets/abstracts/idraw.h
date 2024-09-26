#pragma once
#ifndef IDRAW_H
#define IDRAW_H
#include <assets/primitives.h>

namespace CE::Assets {
    struct iDraw {
        virtual ~iDraw() = default;
        virtual void draw(const DrawInfo& info) = 0;
    };
}
#endif
