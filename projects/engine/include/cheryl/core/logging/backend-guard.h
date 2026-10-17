#pragma once

#include <cheryl/export.h>
#include <cstddef>

namespace CE::Logging {
    /** Cumulative observations for one Log, including failed openings and all
     * generations. Fields are sampled independently. Degradation describes the
     * last published generation and clears only after completed close/reopen.
     * Suppressed operations were already queued when their destination failed.
     */
    struct BackendStats {
        std::size_t failed_operations = 0;
        std::size_t callback_failures = 0;
        std::size_t suppressed_operations = 0;
        bool file_degraded = false;
        bool console_degraded = false;
        std::size_t recursive_submissions = 0;
        std::size_t rejected_reentry = 0;
    };
}

namespace CE::Logging::Detail {
    [[nodiscard]] CE_API bool suppress_backend_emission() noexcept;
    CE_API void reject_backend_reentry(const char* operation);
    [[nodiscard]] CE_API bool in_backend() noexcept;
}
