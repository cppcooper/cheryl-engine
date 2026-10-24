#pragma once
#include "drainage.h"

namespace ProcGenDrainageChecks {
    // <=1024 cells. Independent all-pairs cardinal selection and per-source
    // chain tracing, no production neighbor/queue/accumulation helper reused.
    void inspect(ProcGen::DrainageInput input, const ProcGen::Drainage& output);
}
