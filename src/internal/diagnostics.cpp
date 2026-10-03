#include <core/diagnostics.h>

#include <atomic>
#include <limits>

namespace CE::Diagnostics {
    DomainId next_domain_id() noexcept {
        static std::atomic<DomainId> next{1};
        auto value = next.load(std::memory_order_relaxed);
        while (value != std::numeric_limits<DomainId>::max()) {
            if (next.compare_exchange_weak(value, value + 1, std::memory_order_relaxed))
                return value;
        }
        return 0;
    }
}
