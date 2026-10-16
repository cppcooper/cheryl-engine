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
