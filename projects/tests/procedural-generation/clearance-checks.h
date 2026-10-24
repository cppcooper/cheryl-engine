#pragma once

#include "clearance.h"

namespace ProcGenClearanceChecks {
    // Test/consumer only, <=4096 cells. Exhaustive nearest-solid Manhattan
    // minima plus literal exterior distances, then independent pairwise FIFO
    // cardinal flood. Throws detailed diagnostics on mismatch; no transform reuse.
    [[nodiscard]] ProcGen::ClearanceMeasurements inspect(ProcGen::OccupancyView input, const ProcGen::Clearance& field);
}
