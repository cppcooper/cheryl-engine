#include <core/debug-terminal/startup.h>

#include "arguments.h"

#include <cstdio>
#include <exception>

namespace CE::DebugTerminal {
    int run(const int argc, char** argv, const ApplicationMain application, const Start start) {
        if (!start)
            return application(argc, argv);

        Detail::Arguments arguments;
        try {
            arguments = Detail::parse_arguments(argc, argv, CHERYL_TERMINAL_AUTOMATIC != 0);
            if (arguments.help) {
                std::fputs("Debug terminal options:\n"
                           "  --debug-terminal     Display the native output terminal (automatic in Debug)\n"
                           "  --no-debug-terminal  Use inherited output without a native terminal\n\n", stdout);
            }
            if (arguments.enabled && !Detail::discovery_environment())
                start();
        } catch (const std::exception& error) {
            std::fprintf(stderr, "Cheryl Debug terminal unavailable: %s. Using inherited output.\n", error.what());
        }
        if (arguments.values.empty())
            return application(argc, argv);
        return application(static_cast<int>(arguments.values.size() - 1), arguments.values.data());
    }
}
