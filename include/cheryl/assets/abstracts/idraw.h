#pragma once
#ifndef IDRAW_H
#define IDRAW_H
#include <assets/primitives/draw-info.h>

namespace CE::Assets {
    struct iDraw {
        virtual ~iDraw() = default;
        // Legacy immediate drawing for tile values. New simulation code publishes
        // RenderFrame commands; the graphics thread consumes those values instead.
        virtual void draw(const DrawInfo& info) = 0;
    };
}
#endif
