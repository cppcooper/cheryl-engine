#include <core/logging/log.h>
#include <internals/stack-trace-internal.h>

#include <atomic>

namespace CE::LogDetail {
    uint16_t next_log_id() noexcept {
        // This legacy diagnostic serial wraps; it is not a session/domain identity.
        // It publishes no other logger state, so relaxed ordering is sufficient.
        static std::atomic_uint16_t log_counter{0};
        return static_cast<uint16_t>(log_counter.fetch_add(1, std::memory_order_relaxed) + 1);
    }
}

namespace CE {
    std::string stack_trace(void* addr0) noexcept {
        try {
            return TraceDetail::capture<6144>(addr0, addr0 ? 7 : 17, 0);
        } catch (...) {
            // Default construction and moving an empty string require no allocation.
            // Ordinary logging decides how to display unavailable trace information.
            return {};
        }
    }
}
