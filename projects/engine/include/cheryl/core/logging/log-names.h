#pragma once

namespace CE::Logging::Categories {
    // Engine records select these names explicitly, independently of the host's
    // default logger. Each used category owns its rotating file and console gate.
    extern const char engine[];    // "engine": runtime, execution and events.
    extern const char platform[];  // "os-platform": OS window, display and input services.
    extern const char rendering[]; // "rendering": rendering, shaders and native graphics resources.
    extern const char assets[];    // "assets": preparation, publication and reload.
    extern const char memory[];    // "memory": allocator bookkeeping and ownership.

    // Explicit legacy destination for existing CE::Logging::CherylLog/Logger users. Engine-owned
    // operations use the named categories above; they do not create cheryl.log.
    extern const char cheryl[]; // "cheryl".
}
