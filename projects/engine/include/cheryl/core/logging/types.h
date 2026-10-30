#pragma once

#include <cstdio>
#include <filesystem>
#include <functional>

namespace CE::Logging {
    /** Runtime severity gate in increasing urgency; off disables admission, not destination opening. */
    enum class Level { trace, ///< Fine diagnostic detail, subject to compilation policy.
        debug, ///< Development diagnostics.
        info, ///< Ordinary operational messages.
        warn, ///< Recoverable degradation or warning.
        err, ///< Operation failure.
        critical, ///< Critical failure; ordinary critical writes also request a trace.
        off ///< Suppress destination/logger output.
    };

/** Owned file-lifecycle callbacks invoked by backend operations, not marshalled onto the application thread.
     * Paths and FILE pointers are borrowed only for the invocation. Opening callbacks may reject construction; closing failures are contained and reported after delegate locks unwind.
     * Protect captures through final closure. Recursive ordinary logging is suppressed, and blocking/lifecycle reentry rejects.
     * @see [Logging callbacks](cheryl-guide:runtime/logging)
     */
    struct FileEventHandlers {
        // Callbacks borrow the filename and FILE* only during the call. Opening
        // callbacks may reject construction; closing failures are contained and
        // reported after delegate locks unwind. Keep captured dependencies alive
        // through final closure. Ordinary recursive logging is suppressed.
        std::function<void(const std::filesystem::path&)> before_open; ///< Optional callback before opening; exceptions can reject construction.
        std::function<void(const std::filesystem::path&, std::FILE*)> after_open; ///< Optional callback after opening; stream is borrowed and must not be retained/closed.
        std::function<void(const std::filesystem::path&, std::FILE*)> before_close; ///< Optional callback before final stream closure; failures are contained.
        std::function<void(const std::filesystem::path&)> after_close; ///< Optional callback after closure; only the borrowed path is available.
    };
}
