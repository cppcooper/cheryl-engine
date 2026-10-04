#pragma once

#include <cstdint>

namespace CE::Diagnostics {
    using DomainId = std::uint64_t;

    // Process-local, monotonic identities for diagnostics, never addresses/native
    // handles. Zero means exhausted/unavailable; IDs are not persistence keys.
    [[nodiscard]] DomainId next_domain_id() noexcept;

    // Best-effort cumulative observations, not a scheduling/admission contract.
    struct DispatchStats {
        DomainId domain = 0;
        std::uint64_t accepted = 0;
        std::uint64_t completed = 0;
        std::uint64_t cancelled = 0;
        std::uint64_t failures = 0;
        std::uint64_t pending = 0;
        std::uint64_t running = 0; // Detached batch, including not-yet-entered callbacks.
        std::uint64_t peak_pending = 0;
    };
}
