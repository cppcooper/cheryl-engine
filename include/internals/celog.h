#pragma once
#include <core/logging.h>
#include "internal-logs.h"

namespace CE {
    // Explicit legacy "cheryl" destination. Engine-owned operations select their
    // own category; changing the application default does not redirect this facade.
    struct CELog : Logger<ce_log_name> {
        static void init_pattern() { set_pattern("%^[%n:%l]%$ %v"); }
    };
}
