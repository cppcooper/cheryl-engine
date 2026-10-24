#pragma once
#include "elevation.h"

namespace ProcGenElevationChecks {
    // <=4096 cells. Reconstructs anchors via the already pinned integer stream,
    // then checks each field value with signed rational weights and mathematical
    // floor. Production uses unsigned offsets; no interpolation code is shared.
    void inspect(const ProcGen::Elevation& field);
}
