#include "file-storage.h"
#include "fixture.h"
#include "cli-diagnostics.h"

#include <csignal>
#include <iostream>
#include <string_view>

namespace {
    namespace MF = MapFixture;

    struct Progress {
        std::string_view phase = "arguments";
        bool saved = false;
    };

    void usage(std::ostream& output) {
        output << "Usage:\n"
                  "  map_storage_cli round-trip <file>\n"
                  "  map_storage_cli verify <file>\n"
                  "  map_storage_cli --help\n"
                  "round-trip writes the fixed fixture and may replace an existing regular file.\n"
                  "verify only reads and compares; it never creates, rewrites or repairs files.\n"
                  "Both commands require exactly one explicit nonempty path in a caller-owned directory.\n"
                  "File I/O uses the private single-writer Linux profile.\n";
    }

    void check_stdout() {
        std::cout.flush();
        if (!std::cout)
            throw std::ios_base::failure("cannot write stdout");
    }

    void saved_note(const Progress& progress) {
        if (progress.saved)
            std::cerr << "save completed; subsequent failure does not undo that publication\n";
    }

    int unexpected(const Progress& progress, const char* detail) noexcept {
        try {
            std::cerr << "unexpected failure during " << progress.phase << ": " << detail << '\n';
            saved_note(progress);
            std::cerr.flush();
        } catch (...) {
            // The outer process boundary also owns failures while reporting.
        }
        return 1;
    }

    int consume(int argc, char** argv, Progress& progress) {
        if (argc == 2 && std::string_view(argv[1]) == "--help") {
            progress.phase = "output";
            usage(std::cout);
            check_stdout();
            return 0;
        }
        if (argc != 3 || (std::string_view(argv[1]) != "round-trip" && std::string_view(argv[1]) != "verify") ||
            argv[2][0] == '\0') {
            usage(std::cerr);
            std::cerr.flush();
            return std::cerr ? 64 : 1;
        }
        const std::string_view command(argv[1]);
        progress.phase = "fixture";
        const auto expected = MF::fixed_fixture();
        const std::filesystem::path path(argv[2]);
        if (command == "round-trip") {
            progress.phase = "save";
            MF::save(path, expected);
            progress.saved = true;
        }
        progress.phase = "load";
        const auto restored = MF::load(path);
        progress.phase = "compare";
        if (restored != expected) {
            std::cerr << "mismatch: operation=" << command << " origin=" << std::quoted(path.string())
                      << " complete snapshot differs from literal fixture:two-layers\n";
            saved_note(progress);
            std::cerr.flush();
            return std::cerr ? 3 : 1;
        }
        progress.phase = "output";
        std::cout << command << ": map=" << restored.id << " dimensions=" << restored.width << 'x' << restored.height
                  << " layers=[" << restored.layers[0].id << ',' << restored.layers[1].id << "] spawn="
                  << restored.spawns[0].name << '@' << restored.spawns[0].layer << '(' << restored.spawns[0].x
                  << ',' << restored.spawns[0].y << ")\n";
        check_stdout();
        return 0;
    }
}

int main(int argc, char** argv) {
    std::cerr.tie(nullptr); // A failed stdout must not interrupt stderr reporting.
    Progress progress;
    try {
#if defined(SIGPIPE)
        // Let the checked stream state report a broken pipe as exit 1, even
        // after a successful save. This affects only this CLI process.
        if (std::signal(SIGPIPE, SIG_IGN) == SIG_ERR)
            return unexpected(progress, "cannot ignore SIGPIPE");
#endif
        return consume(argc, argv, progress);
    } catch (const MF::Error& error) {
        try {
            MF::Cli::print_error(std::cerr, error);
            saved_note(progress);
            std::cerr.flush();
            return std::cerr ? 2 : 1;
        } catch (...) {
            return unexpected(progress, "cannot report storage error");
        }
    } catch (const std::exception& error) {
        return unexpected(progress, error.what());
    } catch (...) {
        return unexpected(progress, "unknown exception");
    }
}
