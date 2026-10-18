#pragma once

#include <cheryl/export.h>

namespace CE::Logging {
    /** Owns the process logging lifetime on an application thread.
     * Keep an outer guard in main until all contexts, jobs and logging callers
     * (including diagnostics readers) have finished. Nested/overlapping guards share it.
     * The last guard closes every live Log and drains/joins Cheryl's worker
     * before returning, including when Engine is a shared library.
     * Shutdown is terminal: later guards, new Logs and reopen reject. No producer
     * may outlive the last guard. Release retained native loggers/sinks first;
     * dormant Log facades can be observed and destroyed afterward.
     * Do not construct/destroy guards from a logging callback or DLL entry point.
     */
    class ProcessLifetime final {
    public:
        CE_API ProcessLifetime();
        CE_API ~ProcessLifetime() noexcept;
        ProcessLifetime(const ProcessLifetime&) = delete;
        ProcessLifetime& operator=(const ProcessLifetime&) = delete;
        ProcessLifetime(ProcessLifetime&&) = delete;
        ProcessLifetime& operator=(ProcessLifetime&&) = delete;
    };
}

/**
 * @fn CE::Logging::ProcessLifetime::ProcessLifetime()
 * @brief Acquire a shared process logging guard on an application thread.
 * Nested/overlapping guards share one lifetime. Construction after the final lifetime ended rejects; do not acquire from backend callbacks or a DLL entry point.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn CE::Logging::ProcessLifetime::~ProcessLifetime() noexcept
 * @brief Release this guard; the last release closes every live log and joins the shared logging worker.
 * Stop all producers/diagnostics readers and release native logger/sink references first. Shutdown is terminal; later guards, logs and reopen reject. Do not destroy on a backend callback or DLL entry point.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn CE::Logging::ProcessLifetime::ProcessLifetime(const ProcessLifetime&)
 * @brief Copy/move ownership is disabled; create an independent nested guard instead.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn ProcessLifetime& CE::Logging::ProcessLifetime::operator=(const ProcessLifetime&)
 * @brief Copy/move ownership is disabled; create an independent nested guard instead.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn CE::Logging::ProcessLifetime::ProcessLifetime(ProcessLifetime&&)
 * @brief Copy/move ownership is disabled; create an independent nested guard instead.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */

/**
 * @fn ProcessLifetime& CE::Logging::ProcessLifetime::operator=(ProcessLifetime&&)
 * @brief Copy/move ownership is disabled; create an independent nested guard instead.
 * @see [Detailed guide](cheryl-guide:runtime/logging)
 */
