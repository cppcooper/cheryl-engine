#pragma once

namespace CE {
    // Engine records select these names explicitly, independently of the host's
    // default logger. Each used category owns its rotating file and console gate.
    extern const char enginelog[];   // "engine": runtime, execution and events.
    extern const char platformlog[]; // "os-platform": OS window, display and input services.
    extern const char renderlog[];   // "rendering": rendering, shaders and native graphics resources.
    extern const char assetlog[];    // "assets": preparation, publication and reload.
    extern const char memlog[];      // "memory": allocator bookkeeping and ownership.

    // Explicit legacy destination for existing CELog/Logger users. Engine-owned
    // operations use the named categories above; they do not create cheryl.log.
    extern const char ce_log_name[]; // "cheryl".
}
