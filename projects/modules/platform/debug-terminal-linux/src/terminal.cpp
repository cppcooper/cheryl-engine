#include "terminal.h"

#include <cstdlib>
#include <stdexcept>

namespace CE::TerminalDetail {
    std::vector<std::string> viewer_arguments(
        const std::string& emulator,
        const std::filesystem::path& viewer,
        const std::filesystem::path& socket,
        const std::filesystem::path& output,
        const unsigned long owner
    ) {
        std::vector<std::string> result;
        if (emulator == "konsole")
            result = {emulator, "--separate", "-e"};
        else if (emulator == "xterm")
            result = {emulator, "-T", "Cheryl Debug output", "-e"};
        else
            throw std::invalid_argument("Unsupported native terminal emulator");
        result.insert(result.end(), {viewer.string(), "--socket", socket.string(), "--output", output.string(),
                                       "--owner", std::to_string(owner)});
        return result;
    }

    bool desktop_available() noexcept {
        const char* display = std::getenv("DISPLAY");
        const char* wayland = std::getenv("WAYLAND_DISPLAY");
        return (display && *display) || (wayland && *wayland);
    }
}
