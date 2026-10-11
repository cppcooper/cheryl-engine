#include <core/logging/logger.h>

#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

namespace {
    constexpr char probe_name[] = "terminal-probe";

    void finish_probe() {
        std::fputs("exit-record\n", stderr);
        try {
            spdlog::get(probe_name)->warn("exit-record");
        } catch (...) {
            std::fputs("exit-logger-failure\n", stderr);
        }
    }

    bool write_records() {
        std::puts("c-output");
        std::fputs("c-error\n", stderr);
        std::cout << "cpp-output\n";
        std::cerr << "cpp-error\n";
        constexpr std::string_view output = "native-output\n";
        constexpr std::string_view error = "native-error\n";
        if (::write(STDOUT_FILENO, output.data(), output.size()) != static_cast<ssize_t>(output.size()) ||
            ::write(STDERR_FILENO, error.data(), error.size()) != static_cast<ssize_t>(error.size()))
            return false;
        std::thread worker([] { std::fputs("worker-record\n", stderr); });
        worker.join();
        spdlog::get(probe_name)->warn("record");
        return true;
    }
}

int main(const int argc, char** argv) {
    if (argc < 2)
        return 2;
    const std::string_view scenario(argv[1]);
    if (scenario != "normal" && scenario != "hold" && scenario != "crash" && scenario != "burst")
        return 2;

    CE::LogConfig configuration;
    configuration.directory = std::filesystem::current_path() / "logs";
    configuration.rotate_on_open = false;
    configuration.logger_level = configuration.file_level = configuration.console_level = spdlog::level::warn;
    CE::Logger<probe_name>::initialize(spdlog::file_event_handlers{}, configuration);
    CE::Logger<probe_name>::set_pattern("engine-%v");
    if (std::atexit(finish_probe) != 0)
        return 3;
    std::printf("terminal-linked=%d debug=%d\n", CHERYL_DEBUG_TERMINAL_AVAILABLE, CHERYL_TERMINAL_PROBE_DEBUG);
    for (int index = 1; index < argc; ++index)
        std::printf("argument[%d]=%s\n", index, argv[index]);
    if (!write_records())
        return 5;
    if (scenario == "burst") {
        const std::string payload(4096, 'x');
        for (int index = 0; index < 512; ++index)
            std::printf("burst[%d] %s\n", index, payload.c_str());
    }
    if (scenario == "hold") {
        if (std::getchar() == EOF)
            return 4;
        std::fputs("after-close\n", stderr);
        spdlog::get(probe_name)->warn("after-close");
    }
    if (scenario == "crash") {
        std::fputs("abnormal-record\n", stderr);
        static_cast<void>(std::fflush(nullptr));
        std::_Exit(86);
    }
    std::cout << "buffered-tail";
    return 0;
}
