#pragma once
#include <cstdint>
namespace ProcGen::CopyMetrics {
    // Cell-buffer payloads actually copied at instrumented constructors/call sites.
    // Excludes descriptor strings/domain vectors, reports and allocator metadata.
    inline thread_local std::uint64_t baseline_bytes = 0, weather_bytes = 0, soil_bytes = 0;
}
