#pragma once
#include <core/logging.h>
#include "internal-logs.h"

namespace CE::Logging {
    // Explicit legacy "cheryl" destination. Engine-owned operations select their
    // own category; changing the application default does not redirect this facade.
    struct CherylLog : Logger<::CE::Logging::Categories::cheryl> {
        static void init_pattern() { set_pattern("%^[%n:%l]%$ %v"); }
    };
}
