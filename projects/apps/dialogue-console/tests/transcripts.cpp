#include "support.h"

#include <gtest/gtest.h>

#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace {
    namespace G = GateConsole;
    struct Transcript {
        int exit;
        std::string output;
        std::string error;
    };

    Transcript console(GateTesting::Directory& directory, const std::string& name, const std::string& script) {
        const auto input = directory.file(name + ".in");
        const auto output = directory.file(name + ".out");
        const auto error = directory.file(name + ".err");
        GateTesting::write_fixture(input, script);
        posix_spawn_file_actions_t actions;
        int status = posix_spawn_file_actions_init(&actions);
        if (status)
            throw std::system_error(status, std::generic_category(), "spawn actions");
        for (const auto value :
             {posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, input.c_str(), O_RDONLY, 0),
              posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, output.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600),
              posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, error.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600)}) {
            if (value)
                status = value;
        }
        char executable[] = DIALOGUE_CONSOLE_EXECUTABLE;
        char trace[] = "--trace-effects";
        char* arguments[]{executable, trace, nullptr};
        pid_t child = 0;
        if (!status)
            status = posix_spawn(&child, executable, &actions, nullptr, arguments, environ);
        posix_spawn_file_actions_destroy(&actions);
        if (status)
            throw std::system_error(status, std::generic_category(), "spawn console");
        int result = 0;
        pid_t waited;
        do {
            waited = waitpid(child, &result, 0);
        } while (waited < 0 && errno == EINTR);
        if (waited != child)
            throw std::system_error(errno, std::generic_category(), "wait console");
        Transcript transcript{
            WIFEXITED(result) ? WEXITSTATUS(result) : 128 + WTERMSIG(result), GateTesting::contents(output), GateTesting::contents(error)
        };
        // CTest's complete detail log retains successful as well as failed child
        // scripts, stdout/stderr and immediate statuses after fixture cleanup.
        std::cout << "Transcript " << name << "\nstdin:\n"
                  << script << "\nstdout:\n"
                  << transcript.output << "stderr:\n"
                  << transcript.error << "exit: " << transcript.exit << '\n';
        return transcript;
    }

    void success(const Transcript& transcript, unsigned effects) {
        EXPECT_EQ(transcript.exit, 0);
        EXPECT_EQ(transcript.error, "Introduce effects: " + std::to_string(effects) + "\n");
    }
}

TEST(dialogue_console_transcript, admitted) {
    GateTesting::Directory directory;
    const auto result =
        console(directory, "admitted", "show\nnonsense\nchoose 2 absent\nchoose 2 introduce\nchoose 2 introduce\nchoose 3 pass\nquit\n");
    success(result, 1);
    EXPECT_LT(result.output.find("introduce: Introduce yourself"), result.output.find("silent: Stay silent"));
    EXPECT_LT(result.output.find("pass: Ask to enter"), result.output.find("leave: Leave"));
    EXPECT_NE(result.output.find("A guard waits."), std::string::npos);
    EXPECT_NE(result.output.find("The gate is closed."), std::string::npos);
    EXPECT_NE(result.output.find("You enter\n(End)"), std::string::npos);
    EXPECT_NE(result.output.find("Rejected: unknown command"), std::string::npos);
    EXPECT_NE(result.output.find("Rejected: unknown choice"), std::string::npos);
    EXPECT_NE(result.output.find("Rejected: stale presentation"), std::string::npos);
    EXPECT_NE(result.output.find("Goodbye."), std::string::npos);
}

TEST(dialogue_console_transcript, departed) {
    GateTesting::Directory directory;
    const auto result = console(directory, "departed", "choose 1 silent\nchoose 2 pass\nchoose 1 introduce\nshow\nchoose 3 leave\nquit\n");
    success(result, 0);
    EXPECT_NE(result.output.find("pass: Ask to enter [unavailable]"), std::string::npos);
    EXPECT_NE(result.output.find("Rejected: choice unavailable"), std::string::npos);
    EXPECT_NE(result.output.find("Rejected: stale presentation"), std::string::npos);
    EXPECT_NE(result.output.find("You depart\n(End)"), std::string::npos);
    EXPECT_EQ(result.output.find("You enter"), std::string::npos);
}

TEST(dialogue_console_transcript, same_owner_resume) {
    GateTesting::Directory directory;
    const auto path = directory.file("gate save.json");
    const auto result = console(
        directory, "paired", "choose 1 introduce\nsave " + path + "\nshow\nresume " + path + "\nchoose 3 pass\nchoose 4 pass\nquit\n"
    );
    success(result, 1);
    EXPECT_NE(result.output.find("Saved: " + path), std::string::npos);
    EXPECT_NE(result.output.find("Resumed: " + path), std::string::npos);
    EXPECT_NE(result.output.find("Rejected: stale presentation"), std::string::npos);
    EXPECT_NE(result.output.find("You enter"), std::string::npos);
    const auto saved = G::decode(GateTesting::contents(path)).value();
    EXPECT_TRUE(saved.introduced);
    EXPECT_EQ(saved.dialogue.node.value, "gate");
    EXPECT_EQ(saved.dialogue.step, 1);
}

TEST(dialogue_console_transcript, fresh_process_resume) {
    GateTesting::Directory directory;
    const auto path = directory.file("gate.json");
    success(console(directory, "saving", "choose 1 introduce\nsave " + path + "\nquit\n"), 1);
    const auto bytes = GateTesting::contents(path);
    const auto resumed = console(directory, "fresh", "resume " + path + "\nchoose 2 pass\nquit\n");
    success(resumed, 0);
    EXPECT_NE(resumed.output.find("You enter"), std::string::npos);
    EXPECT_EQ(resumed.output.find("[unavailable]"), std::string::npos);
    EXPECT_EQ(GateTesting::contents(path), bytes);
}

TEST(dialogue_console_transcript, failed_io_preserves_offer) {
    GateTesting::Directory directory;
    const auto path = directory.file("invalid.json");
    const auto existing = directory.file("existing.json");
    const std::string broken = "{\"save_schema\":1,\"flags\":{\"introduced\":true}}";
    GateTesting::write_fixture(path, broken);
    GateTesting::write_fixture(existing, "existing bytes");
    const auto result = console(
        directory, "failures",
        "save " + existing + "\nresume " + path + "\nresume " + directory.file("missing.json") +
            "\nchoose 1 introduce\nchoose 2 pass\nquit\n"
    );
    success(result, 1);
    EXPECT_NE(result.output.find("Exclusive create failed"), std::string::npos);
    EXPECT_NE(result.output.find("Missing required property"), std::string::npos);
    EXPECT_NE(result.output.find("Cannot read"), std::string::npos);
    EXPECT_NE(result.output.find("You enter"), std::string::npos);
    EXPECT_EQ(result.output.find("Saved:"), std::string::npos);
    EXPECT_EQ(result.output.find("Resumed:"), std::string::npos);
    EXPECT_EQ(GateTesting::contents(path), broken);
    EXPECT_EQ(GateTesting::contents(existing), "existing bytes");
}

TEST(dialogue_console_transcript, initial_terminal_saves) {
    GateTesting::Directory directory;
    const auto initial = directory.file("initial.json");
    const auto terminal = directory.file("terminal.json");
    const auto result = console(
        directory, "boundaries",
        "save " + initial + "\nchoose 1 silent\nchoose 2 leave\nsave " + terminal + "\nresume " + initial + "\nresume " + terminal +
            "\nquit\n"
    );
    success(result, 0);
    const auto start = G::decode(GateTesting::contents(initial)).value();
    const auto end = G::decode(GateTesting::contents(terminal)).value();
    EXPECT_EQ(start.dialogue.node.value, "approach");
    EXPECT_EQ(start.dialogue.step, 0);
    EXPECT_FALSE(start.introduced);
    EXPECT_EQ(end.dialogue.node.value, "departed");
    EXPECT_EQ(end.dialogue.step, 2);
    EXPECT_FALSE(end.introduced);
    EXPECT_NE(result.output.find("Offer 5\nYou depart"), std::string::npos);
}

TEST(dialogue_console_transcript, quit_eof_bad_syntax) {
    GateTesting::Directory directory;
    const auto eof = console(directory, "eof", "");
    success(eof, 0);
    EXPECT_NE(eof.output.find("A guard waits."), std::string::npos);
    const auto quit = console(directory, "quit", "quit\nchoose 1 introduce\n");
    success(quit, 0);
    EXPECT_NE(quit.output.find("Goodbye."), std::string::npos);
    const auto bad =
        console(directory, "syntax", "choose\nchoose 1\nchoose abc introduce\nchoose 1 introduce extra\nquit extra\nsave\nresume\nquit\n");
    success(bad, 0);
    EXPECT_NE(bad.output.find("use choose OFFER CHOICE"), std::string::npos);
    EXPECT_NE(bad.output.find("Save path is empty"), std::string::npos);
    EXPECT_EQ(bad.output.find("The gate is closed."), std::string::npos);
}
