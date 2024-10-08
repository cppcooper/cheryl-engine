#pragma once
#include <logging.h>
#include "internal-logs.h"

namespace CE {
    struct CELog : Logger<ce_log_name> {
        static void init_pattern() {
            set_pattern("%^[%n:%l]%$ %v");
        }
    };
}
