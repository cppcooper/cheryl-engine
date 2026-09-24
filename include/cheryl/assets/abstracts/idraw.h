#pragma once
#ifndef IDRAW_H
#define IDRAW_H
#include <assets/primitives/draw-info.h>

namespace CE::Assets {
    struct iDraw {
        virtual ~iDraw() = default;
        // TODO: Revisit asset-owned draw submission. Passing per-instance DrawInfo into an asset
        // couples resource objects to rendering policy and makes later batching, sorting, or render
        // queues harder to introduce. Consider a submission layer where assets provide resources.
        // A dedicated render thread would also benefit from immutable draw packets/commands produced
        // outside the GPU owner thread instead of immediate draw calls mutating material/backend state.
        virtual void draw(const DrawInfo& info) = 0;
    };
}
#endif
