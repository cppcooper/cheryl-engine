#pragma once

#include <cheryl/export.h>

namespace CE::Application::DebugTerminal {
    /** Application entry borrowed for the synchronous run(); must be non-null. Arguments are mutable for compatibility. */
    using Main = int (*)(int argc, char** argv);
    /** Optional synchronous module-session start callback; module retains session/shutdown ownership. */
    using Start = void (*)();

    // The module owns its session and shutdown. No callback leaves startup untouched.
    [[nodiscard]] CE_DEBUG_TERMINAL_API int run(int argc, char** argv, Main application, Start start = nullptr);
}

/**
 * @fn int CE::Application::DebugTerminal::run(int argc, char** argv, Main application, Start start)
 * @brief Optionally start terminal capture, then synchronously invoke the application entry.
 * @param argc Application argument count passed to the entry/parser.
 * @param argv Borrowed argument array, valid throughout this call.
 * @param application Non-null application entry; exceptions from this function propagate.
 * @param start Optional module-owned session initializer; nullptr leaves startup/arguments untouched.
 * @return Application return code.
 * With a callback, parse terminal flags, optionally print help and start only when enabled outside discovery mode. Terminal setup failures report to inherited stderr and continue into the application. Filtered arguments live through the application call; module session teardown remains module-owned.
 * @see [Detailed guide](cheryl-guide:modules/debug-terminal-linux)
 */
