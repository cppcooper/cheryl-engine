#pragma once

#include <cheryl/export.h>

namespace CE::Logging::Categories {
    // Engine records select these names explicitly, independently of the host's
    // default logger. Each used category owns its rotating file and console gate.
    extern CE_API const char engine[]; ///< "engine": runtime, execution and event records.
    extern CE_API const char platform[]; ///< "os-platform": window, display and input services.
    extern CE_API const char rendering[]; ///< "rendering": renderer, shaders and native resources.
    extern CE_API const char assets[]; ///< "assets": preparation, publication and replacement.
    extern CE_API const char memory[]; ///< "memory": allocation bookkeeping and ownership.

    // Explicit legacy destination for existing CE::Logging::CherylLog/Logger users. Engine-owned
    // operations use the named categories above; they do not create cheryl.log.
    extern CE_API const char cheryl[]; ///< "cheryl": explicit legacy destination; Engine operations use the named categories instead.
}
