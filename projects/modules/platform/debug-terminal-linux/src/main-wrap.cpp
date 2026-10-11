#include <core/debug-terminal/startup.h>

#include "native.h"
#include "terminal.h"

#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>

extern char** environ;
extern "C" int __real_main(int argc, char** argv);
extern "C" bool ce_debug_terminal_test_process() noexcept __attribute__((weak));
extern "C" bool ce_debug_terminal_test_reports_routed() noexcept __attribute__((weak));

namespace CE::TerminalDetail {
    namespace {
        class File final {
            std::FILE* value_ = nullptr;

        public:
            ~File() {
                if (value_)
                    static_cast<void>(std::fclose(value_));
            }
            [[nodiscard]] std::FILE* get() const noexcept { return value_; }
            void duplicate(const int descriptor) {
                const int copy = ::fcntl(descriptor, F_DUPFD_CLOEXEC, 3);
                if (copy < 0)
                    native_failure("Save terminal reporting stream");
                value_ = ::fdopen(copy, "w");
                if (!value_) {
                    const int error = errno;
                    static_cast<void>(::close(copy));
                    errno = error;
                    native_failure("Open terminal reporting stream");
                }
                if (std::setvbuf(value_, nullptr, _IONBF, 0) != 0)
                    throw std::runtime_error("Cannot configure terminal reporting stream");
            }
        };

        class SpawnActions final {
            posix_spawn_file_actions_t value_{};

        public:
            SpawnActions() {
                const int error = ::posix_spawn_file_actions_init(&value_);
                if (error != 0)
                    throw std::runtime_error(std::string("Initialize viewer launch: ") + std::strerror(error));
            }
            ~SpawnActions() { static_cast<void>(::posix_spawn_file_actions_destroy(&value_)); }
            [[nodiscard]] posix_spawn_file_actions_t* get() noexcept { return &value_; }
            void discard_streams(const int descriptor) {
                for (const int destination : {STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO}) {
                    const int error = ::posix_spawn_file_actions_adddup2(&value_, descriptor, destination);
                    if (error != 0)
                        throw std::runtime_error(std::string("Detach viewer streams: ") + std::strerror(error));
                }
                const int error = ::posix_spawn_file_actions_addclosefrom_np(&value_, 3);
                if (error != 0)
                    throw std::runtime_error(std::string("Detach viewer descriptors: ") + std::strerror(error));
            }
        };

        class SpawnAttributes final {
            posix_spawnattr_t value_{};

        public:
            SpawnAttributes() {
                int error = ::posix_spawnattr_init(&value_);
                if (error != 0)
                    throw std::runtime_error(std::string("Initialize viewer attributes: ") + std::strerror(error));
                error = ::posix_spawnattr_setflags(&value_, POSIX_SPAWN_SETSID);
                if (error != 0) {
                    static_cast<void>(::posix_spawnattr_destroy(&value_));
                    throw std::runtime_error(std::string("Separate viewer lifetime: ") + std::strerror(error));
                }
            }
            ~SpawnAttributes() { static_cast<void>(::posix_spawnattr_destroy(&value_)); }
            [[nodiscard]] posix_spawnattr_t* get() noexcept { return &value_; }
        };

        class Session final {
            const pid_t owner_ = ::getpid();
            std::filesystem::path directory_;
            std::filesystem::path output_path_;
            std::filesystem::path socket_path_;
            Descriptor output_;
            Descriptor original_output_;
            Descriptor original_error_;
            Descriptor listener_;
            Descriptor control_;
            File report_output_;
            File report_error_;
            pid_t viewer_ = -1;
            std::jthread reaper_;
            std::atomic<bool> viewer_closed_ = false;
            std::atomic<bool> finishing_ = false;
            bool stdout_owned_ = false;
            bool stderr_owned_ = false;
            bool committed_ = false;

        public:
            ~Session() { finish(); }
            void commit() noexcept { committed_ = true; }
            [[nodiscard]] pid_t owner() const noexcept { return owner_; }
            [[nodiscard]] std::FILE* report(const bool error) const noexcept {
                return error ? report_error_.get() : report_output_.get();
            }

            void start() {
                original_output_.reset(::fcntl(STDOUT_FILENO, F_DUPFD_CLOEXEC, 3));
                original_error_.reset(::fcntl(STDERR_FILENO, F_DUPFD_CLOEXEC, 3));
                if (original_output_.get() < 0 || original_error_.get() < 0)
                    native_failure("Save process output destinations");
                report_output_.duplicate(original_output_.get());
                report_error_.duplicate(original_error_.get());

                std::array<char, 40> temporary{};
                constexpr std::string_view pattern = "/tmp/cheryl-terminal-XXXXXX";
                std::memcpy(temporary.data(), pattern.data(), pattern.size());
                if (!::mkdtemp(temporary.data()))
                    native_failure("Create terminal capture directory");
                directory_ = temporary.data();
                output_path_ = directory_ / "output.log";
                socket_path_ = directory_ / "control.sock";
                output_.reset(::open(output_path_.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_APPEND | O_CLOEXEC, 0600));
                if (output_.get() < 0)
                    native_failure("Create terminal capture file");
                listener_.reset(::socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0));
                if (listener_.get() < 0)
                    native_failure("Create terminal control socket");
                const auto address = socket_address(socket_path_.string());
                if (::bind(listener_.get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0 ||
                    ::listen(listener_.get(), 1) != 0)
                    native_failure("Listen for terminal viewer");

                launch();
                await_viewer();
                std::cout.flush();
                std::cerr.flush();
                if (std::fflush(nullptr) != 0)
                    throw std::runtime_error("Cannot flush inherited output before terminal capture");
                if (::dup2(output_.get(), STDOUT_FILENO) < 0)
                    native_failure("Capture process stdout");
                stdout_owned_ = true;
                if (::dup2(output_.get(), STDERR_FILENO) < 0)
                    native_failure("Capture process stderr");
                stderr_owned_ = true;
                if (std::setvbuf(stdout, nullptr, _IOLBF, 0) != 0)
                    throw std::runtime_error("Cannot configure live stdout capture");
                if (!send_control(control_.get(), TerminalDetail::started))
                    native_failure("Start terminal output viewer");
                reaper_ = std::jthread([this](const std::stop_token stop) {
                    while (!stop.stop_requested()) {
                        int status = 0;
                        const auto result = ::waitpid(viewer_, &status, WNOHANG);
                        if (result == viewer_ || (result < 0 && errno == ECHILD)) {
                            viewer_closed_.store(true, std::memory_order_relaxed);
                            if (!finishing_.load(std::memory_order_relaxed))
                                std::fprintf(
                                    report_error_.get(),
                                    "Cheryl Debug viewer closed; process output continues in %s until shutdown.\n", output_path_.c_str()
                                );
                            return;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(40));
                    }
                });
            }

        private:
            void launch() {
                Descriptor null_stream(::open("/dev/null", O_RDWR | O_CLOEXEC));
                if (null_stream.get() < 0)
                    native_failure("Open detached viewer streams");
                SpawnActions actions;
                actions.discard_streams(null_stream.get());
                SpawnAttributes attributes;
                constexpr std::string_view configured = CHERYL_TERMINAL_EMULATOR;
                for (const std::string emulator : {"konsole", "xterm"}) {
                    if (configured != "auto" && configured != emulator)
                        continue;
                    auto arguments = viewer_arguments(emulator, CHERYL_TERMINAL_VIEWER, socket_path_, output_path_,
                        static_cast<unsigned long>(owner_));
                    std::vector<char*> values;
                    values.reserve(arguments.size() + 1);
                    for (auto& value : arguments)
                        values.push_back(value.data());
                    values.push_back(nullptr);
                    const int error = ::posix_spawnp(&viewer_, emulator.c_str(), actions.get(), attributes.get(), values.data(), environ);
                    if (error == 0)
                        return;
                    if (error != ENOENT && error != EACCES)
                        throw std::runtime_error(std::string("Launch terminal viewer: ") + std::strerror(error));
                }
                throw std::runtime_error("No supported terminal emulator is available (konsole or xterm)");
            }

            void await_viewer() {
                constexpr auto timeout = std::chrono::milliseconds(2000);
                const auto deadline = std::chrono::steady_clock::now() + timeout;
                while (std::chrono::steady_clock::now() < deadline) {
                    pollfd channel{.fd = listener_.get(), .events = POLLIN, .revents = 0};
                    const int result = ::poll(&channel, 1, 40);
                    if (result < 0 && errno != EINTR)
                        native_failure("Wait for terminal viewer");
                    if (result > 0 && (channel.revents & POLLIN)) {
                        control_.reset(::accept4(listener_.get(), nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK));
                        if (control_.get() < 0)
                            native_failure("Accept terminal viewer");
                        ucred peer{};
                        socklen_t size = sizeof(peer);
                        if (::getsockopt(control_.get(), SOL_SOCKET, SO_PEERCRED, &peer, &size) != 0 || peer.uid != ::geteuid())
                            throw std::runtime_error("Terminal viewer credentials do not match the process owner");
                        pollfd acknowledgment{.fd = control_.get(), .events = POLLIN, .revents = 0};
                        if (::poll(&acknowledgment, 1, 500) <= 0)
                            throw std::runtime_error("Terminal viewer did not acknowledge startup");
                        char message = 0;
                        if (::recv(control_.get(), &message, 1, 0) != 1 || message != ready)
                            throw std::runtime_error("Terminal viewer returned an invalid startup acknowledgment");
                        listener_.reset();
                        return;
                    }
                    int status = 0;
                    const auto exited = ::waitpid(viewer_, &status, WNOHANG);
                    if (exited == viewer_ || (exited < 0 && errno == ECHILD)) {
                        viewer_ = -1;
                        throw std::runtime_error("Terminal emulator exited before its viewer connected");
                    }
                }
                throw std::runtime_error("Terminal viewer startup timed out");
            }

            void restore(const int stream, const Descriptor& original, bool& owned) noexcept {
                if (!owned)
                    return;
                struct stat current{};
                struct stat capture{};
                if (::fstat(stream, &current) == 0 && ::fstat(output_.get(), &capture) == 0 &&
                    current.st_dev == capture.st_dev && current.st_ino == capture.st_ino)
                    static_cast<void>(::dup2(original.get(), stream));
                owned = false;
            }

            void finish() noexcept {
                if (::getpid() != owner_)
                    return;
                finishing_.store(true, std::memory_order_relaxed);
                try {
                    if (stdout_owned_ || stderr_owned_) {
                        std::cout.flush();
                        std::cerr.flush();
                        if (std::fflush(nullptr) != 0 && report_error_.get())
                            std::fputs("Cheryl Debug terminal: captured output could not be fully flushed.\n", report_error_.get());
                    }
                } catch (...) {
                    if (report_error_.get())
                        std::fputs("Cheryl Debug terminal: captured C++ output could not be fully flushed.\n", report_error_.get());
                }
                restore(STDOUT_FILENO, original_output_, stdout_owned_);
                restore(STDERR_FILENO, original_error_, stderr_owned_);
                const bool notified = control_.get() >= 0 && send_control(control_.get(), normal_exit);
                control_.reset();
                if (reaper_.joinable()) {
                    reaper_.request_stop();
                    reaper_.join();
                }
                if (!committed_ && viewer_ > 0) {
                    int status = 0;
                    pid_t result;
                    do {
                        result = ::waitpid(viewer_, &status, WNOHANG);
                    } while (result < 0 && errno == EINTR);
                    if (result == 0) {
                        if (::kill(viewer_, SIGKILL) == 0 || errno == ESRCH) {
                            do {
                                result = ::waitpid(viewer_, &status, 0);
                            } while (result < 0 && errno == EINTR);
                        } else if (report_error_.get()) {
                            std::fputs("Cheryl Debug terminal: failed launcher could not be terminated.\n", report_error_.get());
                        }
                    }
                }
                if (!notified || !committed_ || viewer_closed_.load(std::memory_order_relaxed)) {
                    if (!output_path_.empty())
                        static_cast<void>(::unlink(output_path_.c_str()));
                    if (!socket_path_.empty())
                        static_cast<void>(::unlink(socket_path_.c_str()));
                    if (!directory_.empty())
                        static_cast<void>(::rmdir(directory_.c_str()));
                }
            }
        };

        std::atomic<Session*> session = nullptr;

        void finish_session() noexcept {
            const auto current = session.load(std::memory_order_acquire);
            if (current && current->owner() == ::getpid())
                delete session.exchange(nullptr, std::memory_order_acq_rel);
        }

        void begin_session() {
            if (!desktop_available())
                return;
            if (ce_debug_terminal_test_process && ce_debug_terminal_test_process() &&
                (!ce_debug_terminal_test_reports_routed || !ce_debug_terminal_test_reports_routed()))
                throw std::runtime_error("The supplied GoogleTest target has no terminal report-routing capability");
            auto candidate = std::make_unique<Session>();
            candidate->start();
            // Lazy engine/logger owners registered later finish before this callback.
            if (std::atexit(finish_session) != 0)
                throw std::runtime_error("Cannot register terminal shutdown");
            candidate->commit();
            session.store(candidate.release(), std::memory_order_release);
        }
    }
}

extern "C" std::FILE* ce_debug_terminal_report_stream(const int error) noexcept {
    const auto current = CE::TerminalDetail::session.load(std::memory_order_acquire);
    if (!current || current->owner() != ::getpid())
        return nullptr;
    return current->report(error != 0);
}

extern "C" int __wrap_main(const int argc, char** argv) {
    return CE::DebugTerminal::run(argc, argv, __real_main, CE::TerminalDetail::begin_session);
}
