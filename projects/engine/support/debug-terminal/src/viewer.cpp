#include "native.h"
#include "terminal.h"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>

#include <array>
#include <charconv>
#include <csignal>
#include <cstdio>
#include <filesystem>
#include <string_view>

namespace {
    volatile std::sig_atomic_t closed = 0;

    void close_viewer(int) { closed = 1; }

    struct Options {
        std::filesystem::path socket;
        std::filesystem::path output;
        unsigned long owner = 0;
    };

    Options arguments(const int argc, char** argv) {
        Options result;
        for (int index = 1; index < argc; index += 2) {
            if (index + 1 >= argc)
                throw std::invalid_argument("Missing viewer option value");
            const std::string_view key(argv[index]);
            const std::string_view value(argv[index + 1]);
            if (key == "--socket")
                result.socket = value;
            else if (key == "--output")
                result.output = value;
            else if (key == "--owner") {
                const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result.owner);
                if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || result.owner == 0)
                    throw std::invalid_argument("Invalid viewer owner");
            } else {
                throw std::invalid_argument("Unknown viewer option");
            }
        }
        if (result.socket.empty() || result.output.empty() || result.owner == 0)
            throw std::invalid_argument("Viewer requires socket, output and owner options");
        if (result.output.parent_path() != result.socket.parent_path() || result.output.filename() != "output.log" ||
            result.socket.filename() != "control.sock")
            throw std::invalid_argument("Viewer paths must identify one owned terminal directory");
        return result;
    }

    enum class Drain { empty, pending, closed };

    Drain drain(const int input) {
        std::array<char, 8192> bytes{};
        // Revisit control messages even while writers continuously extend the file.
        for (int chunk = 0; chunk < 64 && !closed; ++chunk) {
            const auto count = ::read(input, bytes.data(), bytes.size());
            if (count < 0) {
                if (errno == EINTR)
                    continue;
                CE::TerminalDetail::native_failure("Read captured output");
            }
            if (count == 0)
                return Drain::empty;
            if (std::fwrite(bytes.data(), 1, static_cast<std::size_t>(count), stdout) != static_cast<std::size_t>(count))
                return Drain::closed;
            if (std::fflush(stdout) != 0)
                return Drain::closed;
        }
        return closed ? Drain::closed : Drain::pending;
    }

    void remove_directory(const Options& options) noexcept {
        static_cast<void>(::unlink(options.output.c_str()));
        static_cast<void>(::unlink(options.socket.c_str()));
        static_cast<void>(::rmdir(options.output.parent_path().c_str()));
    }

    int run(const Options& options) {
        using namespace CE::TerminalDetail;
        Descriptor control(::socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0));
        if (control.get() < 0)
            native_failure("Create viewer control socket");
        const auto address = socket_address(options.socket.string());
        if (::connect(control.get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0)
            native_failure("Connect viewer control socket");
        if (!send_control(control.get(), ready))
            native_failure("Acknowledge viewer startup");
        char message = 0;
        if (::recv(control.get(), &message, 1, 0) != 1 || message != started)
            return 0;

        Descriptor output(::open(options.output.c_str(), O_RDONLY | O_CLOEXEC));
        if (output.get() < 0)
            native_failure("Open captured output");
        std::printf("\033]0;Cheryl Debug output\007Cheryl Debug output (process %lu)\n", options.owner);
        static_cast<void>(std::fflush(stdout));

        struct sigaction action{};
        action.sa_handler = close_viewer;
        static_cast<void>(::sigemptyset(&action.sa_mask));
        if (::sigaction(SIGHUP, &action, nullptr) != 0 || ::sigaction(SIGINT, &action, nullptr) != 0 ||
            ::sigaction(SIGTERM, &action, nullptr) != 0)
            native_failure("Install viewer closure handlers");

        bool retained = false;
        bool normal = false;
        bool announced = false;
        while (!closed) {
            const auto drained = drain(output.get());
            if (drained == Drain::closed || (normal && drained == Drain::empty))
                break;
            if (retained && drained == Drain::empty && !announced) {
                std::printf("\n[Cheryl process ended without normal shutdown; close this window after inspection.]\n");
                static_cast<void>(std::fflush(stdout));
                announced = true;
            }
            pollfd channel{.fd = retained || normal ? -1 : control.get(), .events = POLLIN, .revents = 0};
            const int result = ::poll(&channel, 1, drained == Drain::pending ? 0 : 50);
            if (result < 0) {
                if (errno == EINTR)
                    continue;
                native_failure("Wait for viewer output");
            }
            if (result > 0) {
                const auto received = ::recv(control.get(), &message, 1, MSG_DONTWAIT);
                if (received == 1 && message == normal_exit) {
                    normal = true;
                } else if (received == 0 || (received < 0 && errno != EINTR && errno != EAGAIN)) {
                    retained = true;
                }
            }
        }
        // An active writer keeps its capture file after manual window closure.
        if (normal || retained)
            remove_directory(options);
        return 0;
    }
}

int main(const int argc, char** argv) {
    try {
        return run(arguments(argc, argv));
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Cheryl Debug viewer: %s\n", error.what());
        return 1;
    }
}
