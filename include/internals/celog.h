#pragma once
#include <logging.h>

namespace CE {
    extern const char ce_log_name[];// = "cheryl";
    struct CELog : Logger<ce_log_name> {
        static void init_pattern() {
            set_pattern("%^[%n:%l]%$ %v");
        }
    };
}
