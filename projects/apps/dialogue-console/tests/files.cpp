#include "support.h"

#include <gtest/gtest.h>

#include <array>
#include <string>

namespace {
    namespace G = GateConsole;
    namespace fs = std::filesystem;
}

TEST(dialogue_console_files, fresh_roundtrip) {
    GateTesting::Directory directory;
    const auto path = directory.file("gate.json");
    std::size_t effects = 0;
    G::Encounter encounter(&effects);
    ASSERT_TRUE(encounter.choose(encounter.view()->choices[0].selection));
    const auto before = encounter.capture().value();
    const auto old_pass = encounter.view()->choices[0].selection;
    ASSERT_TRUE(G::save_fresh(encounter, path));
    EXPECT_EQ(encounter.capture().value(), before);
    EXPECT_EQ(G::decode(G::read_save(path).value()).value(), before);
    ASSERT_TRUE(encounter.choose(old_pass));
    ASSERT_TRUE(G::resume_file(encounter, path));
    EXPECT_EQ(encounter.capture().value(), before);
    EXPECT_EQ(encounter.choose(old_pass).error(), G::D::SessionError::WrongIncarnation);
    ASSERT_TRUE(encounter.choose(encounter.view()->choices[0].selection));
    EXPECT_EQ(effects, 1);
    EXPECT_EQ(GateTesting::contents(path), G::encode(before));
}

TEST(dialogue_console_files, existing_files_links) {
    GateTesting::Directory directory;
    const auto target = directory.file("existing.json");
    const std::string original("original\0bytes\xff", 15);
    GateTesting::write_fixture(target, original);
    G::Encounter encounter;
    const auto before = encounter.capture().value();
    EXPECT_FALSE(G::save_fresh(encounter, target));
    EXPECT_EQ(GateTesting::contents(target), original);
    const auto link = directory.file("leaf-link.json");
    fs::create_symlink(target, link);
    EXPECT_FALSE(G::save_fresh(encounter, link));
    EXPECT_TRUE(fs::is_symlink(link));
    EXPECT_EQ(fs::read_symlink(link), target);
    EXPECT_EQ(GateTesting::contents(target), original);
    const auto absent = directory.file("absent.json");
    const auto dangling = directory.file("dangling.json");
    fs::create_symlink(absent, dangling);
    EXPECT_FALSE(G::save_fresh(encounter, dangling));
    EXPECT_TRUE(fs::is_symlink(dangling));
    EXPECT_FALSE(fs::exists(absent));
    EXPECT_FALSE(G::save_fresh(encounter, directory.file(".")));
    EXPECT_EQ(encounter.capture().value(), before);
}

TEST(dialogue_console_files, invalid_missing_paths) {
    GateTesting::Directory directory;
    G::Encounter encounter;
    const auto before = encounter.capture().value();
    const auto token = encounter.view()->choices[0].selection;
    const auto prefix = directory.file("must-not-create");
    const std::string nul_path = prefix + '\0' + "suffix";
    for (const auto& path : {std::string(), nul_path}) {
        EXPECT_FALSE(G::save_fresh(encounter, path));
        EXPECT_FALSE(G::resume_file(encounter, path));
    }
    EXPECT_FALSE(G::resume_file(encounter, directory.file("missing.json")));
    EXPECT_FALSE(G::save_fresh(encounter, directory.file("missing/child.json")));
    EXPECT_FALSE(fs::exists(prefix));
    EXPECT_EQ(encounter.capture().value(), before);
    ASSERT_TRUE(encounter.choose(token));
}

TEST(dialogue_console_files, deterministic_failures) {
    GateTesting::Directory directory;
    const std::array<std::pair<const char*, G::Testing::FileFailures>, 4> cases{
        {{"Exclusive create failed", {.open = true}},
         {"Write failed", {.partial_write = true}},
         {"Flush failed", {.flush = true}},
         {"Close failed", {.close = true}}}
    };
    unsigned index = 0;
    for (const auto& [diagnostic, fault] : cases) {
        SCOPED_TRACE(diagnostic);
        const auto path = directory.file(std::to_string(++index) + ".json");
        std::size_t effects = 0;
        G::Encounter encounter(&effects);
        const auto before = encounter.capture().value();
        const auto token = encounter.view()->choices[0].selection;
        const auto result = G::save_fresh(encounter, path, fault);
        ASSERT_FALSE(result);
        EXPECT_NE(result.error().find(diagnostic), std::string::npos);
        EXPECT_NE(result.error().find(path), std::string::npos);
        EXPECT_FALSE(fs::exists(path));
        EXPECT_EQ(encounter.capture().value(), before);
        EXPECT_EQ(effects, 0);
        ASSERT_TRUE(encounter.choose(token));
        EXPECT_EQ(effects, 1);

        const auto existing = directory.file(std::to_string(index) + "-existing.json");
        GateTesting::write_fixture(existing, "do not clean up unowned output");
        EXPECT_FALSE(G::save_fresh(encounter, existing, fault));
        EXPECT_EQ(GateTesting::contents(existing), "do not clean up unowned output");
    }
}

TEST(dialogue_console_files, owned_cleanup_failure) {
    GateTesting::Directory directory;
    const auto path = directory.file("incomplete.json");
    G::Encounter encounter;
    const auto before = encounter.capture().value();
    const auto token = encounter.view()->choices[0].selection;
    const auto bytes = G::encode(before);
    const auto result = G::save_fresh(encounter, path, {.partial_write = true, .cleanup = true});
    ASSERT_FALSE(result);
    EXPECT_NE(result.error().find("Write failed"), std::string::npos);
    EXPECT_NE(result.error().find("Owned incomplete output cleanup failed"), std::string::npos);
    EXPECT_NE(result.error().find(path), std::string::npos);
    EXPECT_EQ(GateTesting::contents(path), bytes.substr(0, bytes.size() / 2));
    EXPECT_FALSE(G::save_fresh(encounter, path));
    EXPECT_EQ(GateTesting::contents(path), bytes.substr(0, bytes.size() / 2));
    EXPECT_EQ(encounter.capture().value(), before);
    ASSERT_TRUE(encounter.choose(token));
    EXPECT_FALSE(G::resume_file(encounter, path)); // An incomplete file is not a checkpoint.
}

TEST(dialogue_console_files, cleanup_only_after_failure) {
    GateTesting::Directory directory;
    G::Encounter encounter;
    const auto path = directory.file("complete.json");
    EXPECT_TRUE(G::save_fresh(encounter, path, {.cleanup = true}));
    EXPECT_EQ(GateTesting::contents(path), G::encode(encounter.capture().value()));
    const auto absent = directory.file("never-owned.json");
    const auto result = G::save_fresh(encounter, absent, {.open = true, .cleanup = true});
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error().find("cleanup"), std::string::npos);
    EXPECT_FALSE(fs::exists(absent));
}
