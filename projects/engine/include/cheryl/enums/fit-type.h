#pragma once

namespace CE::Memory {
    /** Capacity-growth choice used by block allocation and cache reservations. */
    enum FitPolicy {
        exact,  ///< Request exactly the selected length; no growth margin.
        larger, ///< Add the configured fixed growth margin.
        greedy  ///< Scale by the configured growth factor, then add the fixed margin.
    };
}
