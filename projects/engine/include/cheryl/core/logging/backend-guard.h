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
        std::size_t failed_operations = 0; ///< Cumulative destination operations that failed, including opening/generations.
        std::size_t callback_failures = 0; ///< Cumulative exceptions from file-lifecycle callbacks.
        std::size_t suppressed_operations = 0; ///< Already queued operations suppressed after destination degradation.
        bool file_degraded = false; ///< Whether the last published file generation degraded; clears after completed close/reopen.
        bool console_degraded = false; ///< Whether the last published console generation degraded.
        std::size_t recursive_submissions = 0; ///< Recursive ordinary submissions suppressed inside backend activity.
        std::size_t rejected_reentry = 0; ///< Rejected blocking/lifecycle reentry attempts from backend callbacks.
    };
}

namespace CE::Logging::Detail {
    [[nodiscard]] CE_API bool suppress_backend_emission() noexcept;
    CE_API void reject_backend_reentry(const char* operation);
    [[nodiscard]] CE_API bool in_backend() noexcept;
}
