#include <gtest/gtest.h>

#include <array>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <spawn.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <system_error>
#include <vector>

extern char** environ;
namespace {
    struct Child {
        int code;
        std::string output, error;
    };
    struct Directory {
        std::filesystem::path path;
        Directory() {
            char name[] = "offline-story-XXXXXX";
            const auto made = mkdtemp(name);
            if (!made)
                throw std::system_error(errno, std::generic_category(), "create child workspace");
            path = made;
        }
        ~Directory() {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    };
    std::string read(const std::filesystem::path& file) {
        std::ifstream stream(file, std::ios::binary);
        if (!stream)
            throw std::runtime_error("Missing child output");
        std::array<char, 65537> bytes{};
        stream.read(bytes.data(), bytes.size());
        if (stream.gcount() == static_cast<std::streamsize>(bytes.size()) || stream.bad())
            throw std::runtime_error("Child output exceeds 64 KiB");
        return {bytes.data(), static_cast<std::size_t>(stream.gcount())};
    }
    Child child(std::vector<std::string> options) {
        Directory dir;
        const auto input = dir.path / "input", output = dir.path / "output", error = dir.path / "error";
        std::ofstream(input).close();
        posix_spawn_file_actions_t actions;
        int code = posix_spawn_file_actions_init(&actions);
        if (code)
            throw std::system_error(code, std::generic_category(), "spawn init");
        for (const auto result :
             {posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, input.c_str(), O_RDONLY, 0),
              posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, output.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600),
              posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, error.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600)})
            if (result)
                code = result;
        char executable[] = STORY_OFFLINE_EXECUTABLE;
        std::vector<char*> argv{executable};
        for (auto& option : options)
            argv.push_back(option.data());
        argv.push_back(nullptr);
        pid_t pid = 0;
        if (!code)
            code = posix_spawn(&pid, executable, &actions, nullptr, argv.data(), environ);
        posix_spawn_file_actions_destroy(&actions);
        if (code)
            throw std::system_error(code, std::generic_category(), "spawn child");
        int status = 0;
        pid_t waited;
        do {
            waited = waitpid(pid, &status, 0);
        } while (waited < 0 && errno == EINTR);
        if (waited != pid)
            throw std::system_error(errno, std::generic_category(), "wait child");
        Child result{WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status), read(output), read(error)};
        std::cout << "Offline child arguments:";
        for (const auto& option : options)
            std::cout << " [" << option << ']';
        std::cout << "\nstdout:\n" << result.output << "stderr:\n" << result.error << "exit: " << result.code << '\n';
        return result;
    }
    std::size_t count(std::string_view text, std::string_view word) {
        std::size_t found = 0, at = 0;
        while ((at = text.find(word, at)) != std::string_view::npos) {
            ++found;
            at += word.size();
        }
        return found;
    }
}

TEST(story_offline_transcript, paid_restore) {
    const auto result = child({"tide", "pay", "0", "restore"});
    ASSERT_EQ(result.code, 0);
    EXPECT_TRUE(result.error.empty());
    EXPECT_NE(result.output.find("Proposed [manually authored; no model/provider]: tide"), std::string::npos);
    EXPECT_NE(result.output.find("Accepted: story-tide-flavour-0 revision=1 mechanical-actions=3 flavour=0"), std::string::npos);
    EXPECT_LT(
        result.output.find("pay: Pay three coins [available]"), result.output.find("repair: Offer two planks to patch the hull [available]")
    );
    EXPECT_NE(result.output.find("NPC prose: Mara pockets the fare. Your seat is ready."), std::string::npos);
    EXPECT_NE(result.output.find("Restored paired boundary; old selection rejected; effects=1"), std::string::npos);
    const std::string expected =
        "End: Destination reached.\nActual facts: coins=5 planks=3 passage=1 licensed=1 paid=1 repaired=0 declined=0\n"
        "Observer mara: knowledge=saw-fare belief-unlicensed=0 attitude=1\n"
        "Observer ivo: knowledge=heard-passage belief-unlicensed=1 attitude=-1\n"
        "Observer lena: knowledge=unaware belief-unlicensed=0 attitude=0\nEffects in this owner: 1\n";
    EXPECT_TRUE(result.output.ends_with(expected));
}

TEST(story_offline_transcript, repair_decline) {
    const auto repair = child({"storm", "repair", "1", "restore"});
    ASSERT_EQ(repair.code, 0);
    EXPECT_TRUE(repair.error.empty());
    EXPECT_NE(repair.output.find("NPC prose: Your board holds. Mara's gratitude outlasts the crossing."), std::string::npos);
    EXPECT_NE(repair.output.find("Actual facts: coins=8 planks=2 passage=1 licensed=1 paid=0 repaired=1 declined=0"), std::string::npos);
    EXPECT_NE(repair.output.find("knowledge=saw-repair belief-unlicensed=0 attitude=4"), std::string::npos);
    EXPECT_NE(repair.output.find("knowledge=heard-passage belief-unlicensed=1 attitude=-3"), std::string::npos);
    const auto decline = child({"tide", "decline"});
    ASSERT_EQ(decline.code, 0);
    EXPECT_TRUE(decline.error.empty());
    EXPECT_NE(
        decline.output.find("End: You remain on shore.\nActual facts: coins=8 planks=3 passage=0 licensed=0 paid=0 repaired=0 declined=1"),
        std::string::npos
    );
}

TEST(story_offline_transcript, refusal) {
    const auto drained = child({"tide", "pay", "0", "drain"});
    ASSERT_EQ(drained.code, 3);
    EXPECT_TRUE(drained.error.empty());
    EXPECT_NE(drained.output.find("pay: Pay three coins [available]"), std::string::npos);
    EXPECT_NE(drained.output.find("Choice rejected: unavailable"), std::string::npos);
    EXPECT_NE(drained.output.find("Actual facts: coins=0 planks=0 passage=0 licensed=0 paid=0 repaired=0 declined=0"), std::string::npos);
    EXPECT_TRUE(drained.output.ends_with("Effects in this owner: 0\n"));
    for (const auto& options :
         {std::vector<std::string>{"unknown", "pay"}, std::vector<std::string>{"tide", "invent-gold"},
          std::vector<std::string>{"tide", "pay", "7"}, std::vector<std::string>{"tide"}}) {
        const auto bad = child(options);
        EXPECT_EQ(bad.code, 2);
        EXPECT_TRUE(bad.output.empty());
        EXPECT_NE(bad.error.find("Usage: story-offline"), std::string::npos);
    }
}

TEST(story_offline_transcript, demonstration) {
    const auto result = child({});
    ASSERT_EQ(result.code, 0);
    EXPECT_TRUE(result.error.empty());
    EXPECT_EQ(count(result.output, "Proposed [manually authored; no model/provider]"), 8);
    EXPECT_EQ(count(result.output, "End: Destination reached."), 6);
    EXPECT_EQ(count(result.output, "End: You remain on shore."), 2);
    EXPECT_EQ(count(result.output, "Effects in this owner: 1"), 8);
    EXPECT_EQ(count(result.output, "Restored paired boundary; old selection rejected; effects=1"), 6);
    EXPECT_NE(result.output.find("Three bright circles buy a quiet crossing."), std::string::npos);
    EXPECT_NE(result.output.find("The river roars; five coins keep your place aboard."), std::string::npos);
}

TEST(story_continuation_transcript, routes) {
    const auto paid = child({"--watch", "tide", "pay", "evidence", "0", "restore"});
    ASSERT_EQ(paid.code, 0);
    EXPECT_TRUE(paid.error.empty());
    EXPECT_NE(paid.output.find("Continuation [manually authored; no model/provider]: fare-inquiry"), std::string::npos);
    EXPECT_NE(paid.output.find("Ferry completed: destination ferry-effects=1"), std::string::npos);
    EXPECT_NE(paid.output.find("fare-record: Show the fare record [available]"), std::string::npos);
    EXPECT_NE(paid.output.find("withhold: Keep your explanation to yourself [available]"), std::string::npos);
    EXPECT_NE(paid.output.find("Watch reply: Ivo checks the fare record. I was wrong: your passage was licensed."), std::string::npos);
    EXPECT_NE(paid.output.find("Restored watch pair; old selection rejected; no repeated fare/reward."), std::string::npos);
    EXPECT_NE(
        paid.output.find("Later response: Ivo: I remember the explanation. No second proof or payment is needed."), std::string::npos
    );
    EXPECT_NE(paid.output.find("Actual facts: coins=5 planks=3 passage=1 licensed=1 paid=1 repaired=0 declined=0"), std::string::npos);
    EXPECT_NE(paid.output.find("Observer ivo: knowledge=verified-fare belief-unlicensed=0 attitude=1"), std::string::npos);
    EXPECT_NE(paid.output.find("Observer lena: knowledge=unaware belief-unlicensed=0 attitude=0"), std::string::npos);
    EXPECT_NE(paid.output.find("Effects: ferry=1 watch=1"), std::string::npos);
    EXPECT_NE(paid.output.find("Ivo: I remember your licensed passage. No accusation today."), std::string::npos);
    const auto repair = child({"--watch", "tide", "repair", "evidence"});
    ASSERT_EQ(repair.code, 0);
    EXPECT_TRUE(repair.error.empty());
    EXPECT_NE(repair.output.find("mara-testimony: Ask Mara to vouch for the repair [available]"), std::string::npos);
    EXPECT_NE(repair.output.find("Mara confirms your repair earned licensed passage. Ivo withdraws the suspicion."), std::string::npos);
    EXPECT_NE(repair.output.find("Actual facts: coins=8 planks=1 passage=1 licensed=1 paid=0 repaired=1 declined=0"), std::string::npos);
    EXPECT_NE(repair.output.find("Observer ivo: knowledge=heard-testimony belief-unlicensed=0 attitude=1"), std::string::npos);
    const auto ashore = child({"--watch", "tide", "decline", "evidence"});
    ASSERT_EQ(ashore.code, 0);
    EXPECT_TRUE(ashore.error.empty());
    EXPECT_NE(ashore.output.find("Ferry completed: shore ferry-effects=1"), std::string::npos);
    EXPECT_NE(ashore.output.find("You remain on the original bank; the ferry has left."), std::string::npos);
    EXPECT_NE(ashore.output.find("explain-stay: Explain that you declined the ferry [available]"), std::string::npos);
    EXPECT_NE(ashore.output.find("Actual facts: coins=8 planks=3 passage=0 licensed=0 paid=0 repaired=0 declined=1"), std::string::npos);
    EXPECT_NE(ashore.output.find("Ivo: I remember you stayed ashore. No crossing to question."), std::string::npos);
    EXPECT_EQ(ashore.output.find("You step onto the harbor quay."), std::string::npos);
}

TEST(story_continuation_transcript, withholding_flavour) {
    for (const auto prior : {"pay", "decline"}) {
        const auto withheld = child({"--watch", "tide", prior, "withhold"});
        ASSERT_EQ(withheld.code, 0);
        EXPECT_TRUE(withheld.error.empty());
        EXPECT_NE(withheld.output.find("Watch reply: Ivo lets the question rest; you keep your own counsel."), std::string::npos);
        EXPECT_NE(withheld.output.find("Effects: ferry=1 watch=0"), std::string::npos);
        EXPECT_EQ(withheld.output.find("Later response:"), std::string::npos);
        EXPECT_NE(withheld.output.find("Observer lena: knowledge=unaware belief-unlicensed=0 attitude=0"), std::string::npos);
        if (std::string_view(prior) == "pay")
            EXPECT_NE(withheld.output.find("Observer ivo: knowledge=heard-passage belief-unlicensed=1 attitude=-1"), std::string::npos);
        else
            EXPECT_NE(withheld.output.find("Observer ivo: knowledge=unaware belief-unlicensed=0 attitude=0"), std::string::npos);
    }
    const auto alternate = child({"--watch", "storm", "repair", "evidence", "1", "restore"});
    ASSERT_EQ(alternate.code, 0);
    EXPECT_TRUE(alternate.error.empty());
    EXPECT_NE(alternate.output.find("Mara names the boards you gave. Ivo accepts her testimony."), std::string::npos);
    EXPECT_NE(alternate.output.find("Actual facts: coins=8 planks=2 passage=1 licensed=1 paid=0 repaired=1 declined=0"), std::string::npos);
    EXPECT_NE(alternate.output.find("Effects: ferry=1 watch=1"), std::string::npos);
}

TEST(story_continuation_transcript, refusal) {
    const auto lost = child({"--watch", "tide", "pay", "evidence", "0", "drain"});
    ASSERT_EQ(lost.code, 3);
    EXPECT_TRUE(lost.error.empty());
    EXPECT_NE(lost.output.find("fare-record: Show the fare record [available]"), std::string::npos);
    EXPECT_NE(lost.output.find("Watch choice rejected: unavailable"), std::string::npos);
    EXPECT_NE(lost.output.find("Observer ivo: knowledge=heard-passage belief-unlicensed=1 attitude=-1"), std::string::npos);
    EXPECT_TRUE(lost.output.ends_with("Effects: ferry=1 watch=0\n"));
    for (const auto& options :
         {std::vector<std::string>{"--watch", "unknown", "pay", "evidence"},
          std::vector<std::string>{"--watch", "tide", "teleport", "evidence"},
          std::vector<std::string>{"--watch", "tide", "pay", "mint_gold"},
          std::vector<std::string>{"--watch", "tide", "pay", "evidence", "7"}, std::vector<std::string>{"--watch"}}) {
        const auto bad = child(options);
        EXPECT_EQ(bad.code, 2);
        EXPECT_TRUE(bad.output.empty());
        EXPECT_NE(bad.error.find("Usage: story-offline --watch"), std::string::npos);
    }
}
