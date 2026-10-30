#pragma once

#include <cheryl/export.h>

#include <cstdint>

namespace CE::Diagnostics {
/** Process-local diagnostic identity; zero means unavailable/exhausted, never a persistence or native-handle key.
     * @see [Diagnostic domains](cheryl-guide:runtime/subsystem-diagnostics)
     */
    using DomainId = std::uint64_t;

    // Process-local, monotonic identities for diagnostics, never addresses/native
    // handles. Zero means exhausted/unavailable; IDs are not persistence keys.
    [[nodiscard]] CE_API DomainId next_domain_id() noexcept;

    // Best-effort cumulative observations, not a scheduling/admission contract.
    /** Best-effort cumulative queue observations for one dispatcher, not an admission or completion barrier. */
    struct DispatchStats {
        ::CE::Diagnostics::DomainId domain = 0; ///< Originating dispatcher identity; zero means unavailable.
        std::uint64_t accepted = 0; ///< Requests successfully admitted since creation.
        std::uint64_t completed = 0; ///< Callbacks whose detached execution batch has completed.
        std::uint64_t cancelled = 0; ///< Accepted requests destroyed without invocation during cancellation.
        std::uint64_t failures = 0; ///< Invoked callbacks that completed their futures with an exception.
        std::uint64_t pending = 0; ///< Requests still in the shared queue at the snapshot boundary.
        std::uint64_t running = 0; ///< Requests in a detached batch, including callbacks not yet entered.
        std::uint64_t peak_pending = 0; ///< Greatest observed shared-queue length.
    };
}

/**
 * @fn CE::Diagnostics::DomainId CE::Diagnostics::next_domain_id() noexcept
 * @brief Allocate a process-local monotonically increasing diagnostic ID.
 * @return A new nonzero ID, or zero after exhaustion. Thread-safe; does not identify persistent objects across runs.
 * @see [Detailed guide](cheryl-guide:runtime/subsystem-diagnostics)
 */
