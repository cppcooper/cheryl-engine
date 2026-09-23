#pragma once

#ifndef CHERYL_SANDBOX_BUILD
#include "runtime-engine.h"

namespace CE::Engine {
    struct glEngine final : RuntimeEngine {
        glEngine();
    };
}
#endif
