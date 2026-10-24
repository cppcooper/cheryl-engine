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

TEST(story_bargain_transcript, routes) {
    const auto immediate = child({"--bargain", "ramp", "pay", "immediate", "0", "restore"});
    ASSERT_EQ(immediate.code, 0);
    EXPECT_TRUE(immediate.error.empty());
    EXPECT_NE(immediate.output.find("Bargain [manually authored; no model/provider]: ramp-quay"), std::string::npos);
    EXPECT_NE(immediate.output.find("help-now: Provide material help now [available]"), std::string::npos);
    EXPECT_NE(immediate.output.find("promise: Promise the full later bundle [available]"), std::string::npos);
    EXPECT_NE(immediate.output.find("negotiate: Negotiate a smaller later bundle [unavailable]"), std::string::npos);
    EXPECT_NE(
        immediate.output.find("Restored outcome boundary; old token rejected; no repeated source or cost effect."), std::string::npos
    );
    EXPECT_NE(
        immediate.output.find(
            "End: closing/helped\nRelief actual: coins=5 planks=2 terms=0 resolution=helped due_planks=0 requester=saw-help attitude=3"
        ),
        std::string::npos
    );
    EXPECT_TRUE(immediate.output.ends_with("Effects: ferry=1 watch=1 relief=1\n"));
    const auto negotiated = child({"--bargain", "ramp", "pay", "negotiate", "0", "restore"});
    ASSERT_EQ(negotiated.code, 0);
    EXPECT_TRUE(negotiated.error.empty());
    EXPECT_NE(negotiated.output.find("Bargain node: terms"), std::string::npos);
    EXPECT_NE(negotiated.output.find("negotiate: Negotiate a smaller later bundle [available]"), std::string::npos);
    EXPECT_NE(negotiated.output.find("Learned smaller_due=1"), std::string::npos);
    EXPECT_NE(
        negotiated.output.find("Restored information boundary; old token rejected; no repeated source or information effect."),
        std::string::npos
    );
    EXPECT_NE(
        negotiated.output.find(
            "End: closing/negotiated\nRelief actual: coins=5 planks=3 terms=1 resolution=negotiated due_planks=1 requester=heard-promise attitude=0"
        ),
        std::string::npos
    );
    EXPECT_TRUE(negotiated.output.ends_with("Effects: ferry=1 watch=1 relief=2\n"));
    const auto promise = child({"--bargain", "shelter", "pay", "promise", "0", "restore"});
    ASSERT_EQ(promise.code, 0);
    EXPECT_TRUE(promise.error.empty());
    EXPECT_NE(promise.output.find("A pending obligation is recorded. No material or future reward has been delivered."), std::string::npos);
    EXPECT_NE(
        promise.output.find(
            "End: closing/promised\nRelief actual: coins=5 planks=3 terms=0 resolution=promised due_planks=3 requester=heard-promise attitude=0"
        ),
        std::string::npos
    );
    const auto refused = child({"--bargain", "ramp", "decline", "refuse"});
    ASSERT_EQ(refused.code, 0);
    EXPECT_TRUE(refused.error.empty());
    EXPECT_NE(refused.output.find("Requester: oren; source=original bank"), std::string::npos);
    EXPECT_NE(
        refused.output.find(
            "End: closing/refused\nRelief actual: coins=8 planks=3 terms=0 resolution=refused due_planks=0 requester=heard-refusal attitude=-1"
        ),
        std::string::npos
    );
    EXPECT_NE(refused.output.find("passage=0 licensed=0 paid=0 repaired=0 declined=1"), std::string::npos);
    EXPECT_EQ(refused.output.find("On the harbor quay."), std::string::npos);
    EXPECT_TRUE(refused.output.ends_with("Effects: ferry=1 watch=0 relief=1\n"));
}

TEST(story_bargain_transcript, configurations) {
    const auto shelter = child({"--bargain", "shelter", "pay", "immediate", "1", "restore"});
    ASSERT_EQ(shelter.code, 0);
    EXPECT_TRUE(shelter.error.empty());
    EXPECT_NE(shelter.output.find("Rain is coming through the shelter roof."), std::string::npos);
    EXPECT_NE(
        shelter.output.find("Relief actual: coins=5 planks=1 terms=0 resolution=helped due_planks=0 requester=saw-help attitude=3"),
        std::string::npos
    );
    const auto negotiated = child({"--bargain", "shelter", "pay", "negotiate", "1", "restore"});
    ASSERT_EQ(negotiated.code, 0);
    EXPECT_TRUE(negotiated.error.empty());
    EXPECT_NE(
        negotiated.output.find(
            "Relief actual: coins=5 planks=3 terms=1 resolution=negotiated due_planks=2 requester=heard-promise attitude=0"
        ),
        std::string::npos
    );
    const auto paid = child({"--bargain", "ramp", "pay", "pay", "0", "restore"});
    ASSERT_EQ(paid.code, 0);
    EXPECT_TRUE(paid.error.empty());
    EXPECT_NE(
        paid.output.find(
            "End: closing/paid\nRelief actual: coins=3 planks=3 terms=0 resolution=paid due_planks=0 requester=saw-payment attitude=1"
        ),
        std::string::npos
    );
    const auto bank = child({"--bargain", "ramp", "decline", "immediate"});
    ASSERT_EQ(bank.code, 0);
    EXPECT_TRUE(bank.error.empty());
    EXPECT_NE(bank.output.find("On the original bank. The relief ramp needs a sound brace before nightfall."), std::string::npos);
    EXPECT_NE(
        bank.output.find("Relief actual: coins=8 planks=2 terms=0 resolution=helped due_planks=0 requester=saw-help attitude=3"),
        std::string::npos
    );
    EXPECT_NE(bank.output.find("Observer ivo: knowledge=unaware belief-unlicensed=0 attitude=0"), std::string::npos);
    EXPECT_NE(bank.output.find("Observer lena: knowledge=unaware belief-unlicensed=0 attitude=0"), std::string::npos);
}

TEST(story_bargain_transcript, refusal) {
    const auto drained = child({"--bargain", "ramp", "pay", "immediate", "0", "drain"});
    ASSERT_EQ(drained.code, 3);
    EXPECT_TRUE(drained.error.empty());
    EXPECT_NE(drained.output.find("help-now: Provide material help now [available]"), std::string::npos);
    EXPECT_NE(
        drained.output.find(
            "Bargain choice rejected: unavailable\nRelief actual: coins=0 planks=0 terms=0 resolution=open due_planks=0 requester=unaware attitude=0"
        ),
        std::string::npos
    );
    EXPECT_TRUE(drained.output.ends_with("Effects: ferry=1 watch=1 relief=0\n"));
    for (const auto& options :
         {std::vector<std::string>{"--bargain", "unknown", "pay", "immediate"},
          std::vector<std::string>{"--bargain", "ramp", "teleport", "promise"},
          std::vector<std::string>{"--bargain", "ramp", "pay", "fulfill"},
          std::vector<std::string>{"--bargain", "ramp", "pay", "promise", "7"}, std::vector<std::string>{"--bargain"}}) {
        const auto bad = child(options);
        EXPECT_EQ(bad.code, 2);
        EXPECT_TRUE(bad.output.empty());
        EXPECT_NE(bad.error.find("Usage: story-offline --bargain"), std::string::npos);
    }
}

TEST(story_selection_transcript, selected_play) {
    const auto small = child({"--select-bargain", "1", "small", "immediate"});
    ASSERT_EQ(small.code, 0);
    EXPECT_TRUE(small.error.empty());
    EXPECT_NE(small.output.find("algorithm=1 seed=1 examined=32 compatible=8"), std::string::npos);
    EXPECT_NE(
        small.output.find("request=ramp-quay proposition=ramp-request actor=nera plan=ramp bundle=open flavour=1"), std::string::npos
    );
    EXPECT_NE(small.output.find("Mechanics: material=1 coins=2 pending=2 revised=1 edges=16"), std::string::npos);
    EXPECT_NE(
        small.output.find("Relief actual: coins=5 planks=2 terms=0 resolution=helped due_planks=0 requester=saw-help attitude=3"),
        std::string::npos
    );
    EXPECT_TRUE(small.output.ends_with("Selected end: closing; effects: ferry=1 watch=1 relief=1\n"));
    const auto costly = child({"--select-bargain", "1", "open", "immediate"});
    ASSERT_EQ(costly.code, 0);
    EXPECT_TRUE(costly.error.empty());
    EXPECT_NE(costly.output.find("plan=shelter bundle=open flavour=1"), std::string::npos);
    EXPECT_NE(costly.output.find("coins=5 planks=1 terms=0 resolution=helped"), std::string::npos);
    const auto promise = child({"--select-bargain", "1", "consult", "promise"});
    ASSERT_EQ(promise.code, 0);
    EXPECT_TRUE(promise.error.empty());
    EXPECT_NE(promise.output.find("plan=shelter bundle=explain-first flavour=1"), std::string::npos);
    EXPECT_NE(promise.output.find("Restored selected information pair; old token rejected; no replay."), std::string::npos);
    EXPECT_NE(
        promise.output.find("coins=5 planks=3 terms=1 resolution=promised due_planks=3 requester=heard-promise attitude=0"),
        std::string::npos
    );
    EXPECT_TRUE(promise.output.ends_with("Selected end: closing; effects: ferry=1 watch=1 relief=2\n"));
    const auto negotiated = child({"--select-bargain", "0", "any", "negotiate"});
    ASSERT_EQ(negotiated.code, 0);
    EXPECT_TRUE(negotiated.error.empty());
    EXPECT_NE(
        negotiated.output.find("request=shelter-quay proposition=shelter-request actor=nera plan=ramp bundle=explain-first flavour=1"),
        std::string::npos
    );
    EXPECT_NE(
        negotiated.output.find("coins=5 planks=3 terms=1 resolution=negotiated due_planks=1 requester=heard-promise attitude=0"),
        std::string::npos
    );
    const auto refused = child({"--select-bargain", "0", "any", "refuse"});
    ASSERT_EQ(refused.code, 0);
    EXPECT_TRUE(refused.error.empty());
    EXPECT_NE(
        refused.output.find("coins=5 planks=3 terms=0 resolution=refused due_planks=0 requester=heard-refusal attitude=-1"),
        std::string::npos
    );
    EXPECT_TRUE(refused.output.ends_with("Selected end: closing; effects: ferry=1 watch=1 relief=1\n"));
}

TEST(story_selection_transcript, refusal_and_usage) {
    const std::pair<const char*, const char*> refusals[] = {
        {"empty", "empty-compatible-set"}, {"budget", "incomplete-work-budget"}, {"nodraw", "draw-budget"}
    };
    for (const auto& [profile, reason] : refusals) {
        const auto r = child({"--select-bargain", "0", profile, "immediate"});
        EXPECT_EQ(r.code, 3);
        EXPECT_TRUE(r.error.empty());
        EXPECT_NE(r.output.find(std::string("Selection refused before bargain play: ") + reason), std::string::npos);
        EXPECT_EQ(r.output.find("Selected node:"), std::string::npos);
        EXPECT_EQ(r.output.find("Relief actual:"), std::string::npos);
    }
    for (const auto& args :
         {std::vector<std::string>{"--select-bargain"}, std::vector<std::string>{"--select-bargain", "-1", "any", "promise"},
          std::vector<std::string>{"--select-bargain", "18446744073709551616", "any", "promise"},
          std::vector<std::string>{"--select-bargain", "1x", "any", "promise"},
          std::vector<std::string>{"--select-bargain", "1", "weaken", "promise"},
          std::vector<std::string>{"--select-bargain", "1", "any", "fulfill"}}) {
        const auto r = child(args);
        EXPECT_EQ(r.code, 2);
        EXPECT_TRUE(r.output.empty());
        EXPECT_NE(r.error.find("Usage: story-offline --select-bargain"), std::string::npos);
    }
}

TEST(story_payoff_transcript, fulfilled_configurations) {
    const auto small = child({"--payoff", "1", "promise", "deliver", "restore"});
    ASSERT_EQ(small.code, 0);
    EXPECT_TRUE(small.error.empty());
    EXPECT_NE(small.output.find("plan=ramp actor=nera prior=promise"), std::string::npos);
    EXPECT_NE(small.output.find("Recorded due: 2 planks."), std::string::npos);
    EXPECT_NE(small.output.find("deliver: Deliver the recorded material [unavailable]"), std::string::npos);
    EXPECT_NE(small.output.find("deliver: Deliver the recorded material [available]"), std::string::npos);
    EXPECT_NE(small.output.find("break: Declare the promise broken [available]"), std::string::npos);
    EXPECT_NE(small.output.find("Restored before handoff; no prior charges or favors replayed."), std::string::npos);
    EXPECT_NE(small.output.find("Restored pending handoff pair; old token rejected; no effect replay."), std::string::npos);
    EXPECT_NE(small.output.find("Fresh owner restored resolved pair; no additional delivery or favor."), std::string::npos);
    EXPECT_NE(
        small.output.find("Payoff actual: coins=5 planks=1 ready=1 obligation=fulfilled due=0 delivered=2 favor=1 outcome=fulfilled"),
        std::string::npos
    );
    EXPECT_NE(small.output.find("Recipient: knowledge=checked-delivery attitude=4 believes-broken=0"), std::string::npos);
    EXPECT_NE(small.output.find("Observer lena: knowledge=unaware belief-unlicensed=0 attitude=0"), std::string::npos);
    EXPECT_TRUE(small.output.ends_with("Effects: ferry=1 watch=1 bargain=1 payoff=1\n"));
    const auto large = child({"--payoff", "3", "promise", "deliver"});
    ASSERT_EQ(large.code, 0);
    EXPECT_TRUE(large.error.empty());
    EXPECT_NE(large.output.find("plan=shelter actor=nera prior=promise"), std::string::npos);
    EXPECT_NE(large.output.find("Recorded due: 3 planks."), std::string::npos);
    EXPECT_NE(
        large.output.find("Payoff actual: coins=5 planks=0 ready=1 obligation=fulfilled due=0 delivered=3 favor=1 outcome=fulfilled"),
        std::string::npos
    );
    const auto negotiated = child({"--payoff", "1", "negotiate", "deliver", "restore"});
    ASSERT_EQ(negotiated.code, 0);
    EXPECT_TRUE(negotiated.error.empty());
    EXPECT_NE(
        negotiated.output.find("coins=5 planks=2 ready=1 obligation=fulfilled due=0 delivered=1 favor=1 outcome=fulfilled"),
        std::string::npos
    );
    EXPECT_TRUE(negotiated.output.ends_with("Effects: ferry=1 watch=1 bargain=2 payoff=1\n"));
}

TEST(story_payoff_transcript, breach_refusal_acknowledgment) {
    const auto broken = child({"--payoff", "1", "promise", "break", "restore"});
    ASSERT_EQ(broken.code, 0);
    EXPECT_TRUE(broken.error.empty());
    EXPECT_NE(broken.output.find("coins=5 planks=3 ready=1 obligation=broken due=0 delivered=0 favor=0 outcome=broken"), std::string::npos);
    EXPECT_NE(broken.output.find("Recipient: knowledge=heard-breach attitude=-3 believes-broken=1"), std::string::npos);
    const auto helped = child({"--payoff", "1", "immediate", "acknowledge", "restore"});
    ASSERT_EQ(helped.code, 0);
    EXPECT_TRUE(helped.error.empty());
    EXPECT_NE(
        helped.output.find("coins=5 planks=2 ready=1 obligation=none due=0 delivered=0 favor=0 outcome=acknowledged"), std::string::npos
    );
    EXPECT_NE(helped.output.find("Recipient: knowledge=saw-help attitude=3 believes-broken=0"), std::string::npos);
    const auto paid = child({"--payoff", "1", "pay", "acknowledge"});
    ASSERT_EQ(paid.code, 0);
    EXPECT_TRUE(paid.error.empty());
    EXPECT_NE(
        paid.output.find("coins=3 planks=3 ready=1 obligation=none due=0 delivered=0 favor=0 outcome=acknowledged"), std::string::npos
    );
    const auto refused = child({"--payoff", "1", "refuse", "acknowledge", "bank"});
    ASSERT_EQ(refused.code, 0);
    EXPECT_TRUE(refused.error.empty());
    EXPECT_NE(refused.output.find("actor=oren prior=refuse"), std::string::npos);
    EXPECT_NE(refused.output.find("Original bank handoff."), std::string::npos);
    EXPECT_NE(
        refused.output.find("coins=8 planks=3 ready=1 obligation=none due=0 delivered=0 favor=0 outcome=acknowledged"), std::string::npos
    );
    EXPECT_NE(refused.output.find("Recipient: knowledge=heard-refusal attitude=-1 believes-broken=0"), std::string::npos);
    EXPECT_NE(refused.output.find("passage=0 licensed=0 paid=0 repaired=0 declined=1"), std::string::npos);
    EXPECT_EQ(refused.output.find("Harbor quay handoff."), std::string::npos);
    EXPECT_TRUE(refused.output.ends_with("Effects: ferry=1 watch=0 bargain=1 payoff=1\n"));
    const auto left = child({"--payoff", "1", "promise", "leave"});
    ASSERT_EQ(left.code, 0);
    EXPECT_TRUE(left.error.empty());
    EXPECT_NE(left.output.find("coins=5 planks=3 ready=1 obligation=pending due=2 delivered=0 favor=0 outcome=left"), std::string::npos);
}

TEST(story_payoff_transcript, live_refusal_and_usage) {
    for (const auto& mode : {"drain", "absent"}) {
        const auto r = child({"--payoff", "1", "promise", "deliver", mode});
        EXPECT_EQ(r.code, 3);
        EXPECT_TRUE(r.error.empty());
        EXPECT_NE(r.output.find("Payoff choice rejected before mutation."), std::string::npos);
        EXPECT_NE(r.output.find("obligation=pending due=2 delivered=0 favor=0 outcome=open"), std::string::npos);
        EXPECT_TRUE(r.output.ends_with("Payoff effects: 0\n"));
    }
    for (const auto& args :
         {std::vector<std::string>{"--payoff"}, std::vector<std::string>{"--payoff", "-1", "promise", "deliver"},
          std::vector<std::string>{"--payoff", "1", "refuse", "deliver"},
          std::vector<std::string>{"--payoff", "1", "promise", "acknowledge"},
          std::vector<std::string>{"--payoff", "1", "promise", "invent-favor"},
          std::vector<std::string>{"--payoff", "1", "promise", "deliver", "loop"}}) {
        const auto r = child(args);
        EXPECT_EQ(r.code, 2);
        EXPECT_TRUE(r.output.empty());
        EXPECT_NE(r.error.find("Usage: story-offline --payoff"), std::string::npos);
    }
}
