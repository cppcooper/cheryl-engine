#include <tools/scenarios/command.h>
#include <tests/host/access.h>

#include <gtest/gtest.h>
#include <atomic>
#include <array>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <thread>

namespace {
    namespace Living = CE::LivingWorld;
    namespace Scenarios = Living::Scenarios;
    namespace Core = CE::Tools::Console;
    namespace Script = Core::Scenario;
    class Files {
        static inline std::atomic<unsigned> sequence = 0;
    public:
        CE::Workers::Pool pool{1};
        std::shared_ptr<Scenarios::IoService> io = std::make_shared<Scenarios::IoService>(pool.make_group());
        std::filesystem::path root;
        Files() {
            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            root = std::filesystem::temp_directory_path() / ("cheryl-scenarios-" + std::to_string(stamp) + "-" + std::to_string(++sequence));
            if (!std::filesystem::create_directory(root)) throw std::runtime_error("Cannot create test root");
        }
        ~Files() { io->settle(); std::error_code ignored; std::filesystem::remove_all(root, ignored); }
        void write(std::string_view name, std::string_view body) {
            std::ofstream output(root / name, std::ios::binary); output << body;
            if (!output) throw std::runtime_error("Cannot write scenario test input");
        }
        std::string read(std::string_view name) const {
            std::ifstream input(root / name, std::ios::binary);
            if (!input) throw std::runtime_error("Cannot read scenario test evidence");
            return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
        }
        Scenarios::Config config(std::string script, std::string run = "run") const {
            Scenarios::Config config;
            config.script_root = SCENARIO_FIXTURES; config.asset_root = SCENARIO_ASSETS;
            config.output_root = root; config.script = std::move(script); config.run_id = std::move(run);
            config.build_id = "source-tests-not-runtime-evidence";
            config.io = io;
            return config;
        }
    };
    int batch(Scenarios::Job& job) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (job.disposition() == Script::Disposition::Running && std::chrono::steady_clock::now() < deadline) {
            job.step(); std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return job.batch_exit();
    }
    bool initialize(Scenarios::Job& job) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!job.fixture() && job.disposition() == Script::Disposition::Running && std::chrono::steady_clock::now() < deadline) {
            job.step(); std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return job.fixture() != nullptr;
    }
    bool applied(Scenarios::Job& job) {
        if (!initialize(job)) return false;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (Living::HostTestAccess::image(*job.fixture()->host).record_count == 0 &&
            job.disposition() == Script::Disposition::Running && std::chrono::steady_clock::now() < deadline) {
            job.step(); std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return Living::HostTestAccess::image(*job.fixture()->host).record_count == 1;
    }
    Living::Actor actor(const Scenarios::Job& job, std::uint64_t id) {
        const auto* fixture = job.fixture();
        return fixture->host->actor(fixture->access, fixture->host->entity(id)).value();
    }
    const Living::Detail::Image& image(const Scenarios::Job& job) { return Living::HostTestAccess::image(*job.fixture()->host); }
    constexpr auto prefix = "script.version 1\nfixture.load contact-heal-v1\n";
}

#if defined(__unix__) || defined(__APPLE__)
TEST(ConsoleScenarios, HealUsesRealS1AndDuplicateRetainsOneAction) {
    Files files; Scenarios::Job job(files.config("heal.console"));
    ASSERT_EQ(batch(job), 0) << job.diagnostic();
    EXPECT_TRUE(job.durable()); EXPECT_EQ(actor(job, 2).health, 20u); EXPECT_EQ(actor(job, 1).reserve, 7u);
    EXPECT_EQ(image(job).record_count, 1u);
    ASSERT_GE(job.results().size(), 4u);
    EXPECT_EQ(job.results()[1].request_id, job.results()[2].request_id);
    EXPECT_EQ(job.results()[1].request_id, job.results()[3].request_id);
    EXPECT_TRUE(Scenarios::completed_report(files.root, "run", job.source_id(), "source-tests-not-runtime-evidence"));
    const auto report = Scenarios::read_source(files.root, "run/report.json");
    EXPECT_NE(report.find("\"reference\":\"$outcome.gain\""), std::string::npos);
    EXPECT_NE(report.find("\"domain\":\"operation\""), std::string::npos);
    EXPECT_NE(report.find("\"context_mode\":\"StrictChannels\""), std::string::npos);
    EXPECT_NE(report.find("\"route\":\"Gameplay\",\"request_id\":1"), std::string::npos);
    EXPECT_NE(report.find("\"route\":\"Diagnostic\",\"request_id\":1"), std::string::npos);
    const auto transcript = files.read("run/transcript.txt");
    EXPECT_TRUE(transcript.starts_with(Scenarios::read_source(SCENARIO_FIXTURES, "heal.console") + '\n'));
    EXPECT_TRUE(transcript.ends_with("snapshot " + report + '\n'));
}

TEST(ConsoleScenarios, StrictAuthorityValidatesTheWholeFileBeforeEffects) {
    const std::array<std::string_view, 4> denied{
        "actor.read marin -> hidden\n",
        "set practice.heal_price 1 -> setting\n",
        "observe magic.cast S1 marin -> privileged\n",
        "observe set practice.heal_price 1 -> setting\n"};
    for (const auto command : denied) {
        Files files;
        files.write("denied.console", std::string(prefix) +
            "magic.cast S1 marin -> cast\nexpect $cast.gain == 8\n" + std::string(command) + "finish\n");
        auto config = files.config("denied.console"); config.script_root = files.root;
        Scenarios::Job job(config);
        EXPECT_NE(batch(job), 0);
        ASSERT_EQ(job.disposition(), Script::Disposition::Invalid) << command;
        ASSERT_NE(job.fixture(), nullptr);
        EXPECT_EQ(image(job).record_count, 0u);
        EXPECT_EQ(actor(job, 2).health, 12u);
        EXPECT_EQ(actor(job, 1).reserve, 10u);
        EXPECT_EQ(job.fixture()->host->price(), 3u);
        const auto report = files.read("run/report.json");
        EXPECT_EQ(report.find("\"error_line\":0,"), std::string::npos);
        EXPECT_EQ(report.find("\"error_code\":0,"), std::string::npos);
    }
}

TEST(ConsoleScenarios, ExpectedResourcesRefusalDoesNotDebitOrHeal) {
    Files files; Scenarios::Job job(files.config("refused.console"));
    ASSERT_EQ(batch(job), 0) << job.diagnostic();
    EXPECT_EQ(actor(job, 2).health, 12u); EXPECT_EQ(actor(job, 1).reserve, 2u);
    EXPECT_TRUE(actor(job, 1).can_cast); EXPECT_TRUE(actor(job, 2).willing); EXPECT_TRUE(actor(job, 2).touching);
    EXPECT_EQ(image(job).record_count, 1u);
}

TEST(ConsoleScenarios, AuthoringUsesTheActualGameplayHandlersAndRetainsEditedOperation) {
    Files files; Scenarios::Job job(files.config("authoring.console"));
    ASSERT_EQ(batch(job), 0) << job.diagnostic();
    EXPECT_TRUE(job.durable());
    ASSERT_NE(job.fixture(), nullptr);
    EXPECT_EQ(actor(job, 2).health, 17u); EXPECT_EQ(actor(job, 1).reserve, 7u);
    EXPECT_EQ(image(job).record_count, 1u);
    EXPECT_EQ(Living::HostTestAccess::issued(*job.fixture()->host), 1u);
    EXPECT_EQ(job.assertions().size(), 12u);
    ASSERT_GE(job.results().size(), 7u);
    EXPECT_EQ(job.results()[3].route, Script::Route::Gameplay);
    EXPECT_EQ(job.results()[3].request_id, job.results()[4].request_id);
    EXPECT_EQ(job.results()[3].request_id, job.results()[6].request_id);
    EXPECT_TRUE(Scenarios::completed_report(files.root, "run", job.source_id(), "source-tests-not-runtime-evidence"));
}

TEST(ConsoleScenarios, ExpectedNegativeWrapperRequiresWrongLiteralNonzeroExit) {
    Files files; Scenarios::Job job(files.config("wrong-assertion.console"));
    EXPECT_NE(batch(job), 0); EXPECT_EQ(job.disposition(), Script::Disposition::Failed);
    EXPECT_TRUE(job.durable()); EXPECT_EQ(actor(job, 2).health, 20u); EXPECT_EQ(actor(job, 1).reserve, 7u);
    ASSERT_EQ(job.assertions().size(), 1u);
    EXPECT_EQ(std::get<std::uint64_t>(job.assertions()[0].expected.data), 9u);
    EXPECT_EQ(std::get<std::uint64_t>(job.assertions()[0].actual.data), 8u);
    EXPECT_FALSE(Scenarios::completed_report(files.root, "run", job.source_id(), "source-tests-not-runtime-evidence"));
    const auto report = files.read("run/report.json");
    EXPECT_TRUE(files.read("run/transcript.txt").ends_with("snapshot " + report + '\n'));
    EXPECT_NE(report.find("\"reference\":\"$outcome.gain\""), std::string::npos);
}

TEST(ConsoleScenarios, UnexpectedResourcesRefusalFailsWithoutSecondAttempt) {
    Files files;
    files.write("unexpected.console", "script.version 1\nfixture.load contact-reserve-v1\n"
        "magic.cast S1 marin -> cast\nawait $cast max_steps=4 -> result\nexpect $result.gain == 0\nfinish\n");
    auto config = files.config("unexpected.console"); config.script_root = files.root;
    Scenarios::Job job(config);
    EXPECT_NE(batch(job), 0); EXPECT_EQ(job.disposition(), Script::Disposition::Failed);
    ASSERT_NE(job.fixture(), nullptr);
    EXPECT_EQ(actor(job, 2).health, 12u); EXPECT_EQ(actor(job, 1).reserve, 2u);
    EXPECT_EQ(image(job).record_count, 1u);
    EXPECT_FALSE(Scenarios::completed_report(files.root, "run", job.source_id(), "source-tests-not-runtime-evidence"));
}

TEST(ConsoleScenarios, LaterInvalidFieldPreventsEveryOwnerAction) {
    Files files;
    files.write("invalid.console", std::string(prefix) + "magic.cast S1 marin -> cast\nexpect $cast.missing == 8\nfinish\n");
    auto config = files.config("invalid.console"); config.script_root = files.root;
    Scenarios::Job job(config);
    EXPECT_NE(batch(job), 0); ASSERT_EQ(job.disposition(), Script::Disposition::Invalid);
    ASSERT_NE(job.fixture(), nullptr); EXPECT_EQ(image(job).record_count, 0u);
    EXPECT_EQ(actor(job, 2).health, 12u); EXPECT_EQ(actor(job, 1).reserve, 10u);
}

TEST(ConsoleScenarios, InvalidTypeZeroAssertionsAndRecursionNeverExecute) {
    const std::array<std::string, 3> bodies{
        "magic.cast S1 marin -> cast\nexpect $cast.gain == wrong\nfinish\n",
        "magic.cast S1 marin -> cast\nfinish\n",
        "test.run heal.console nested -> job\nexpect $job.durable == true\nfinish\n"};
    for (const auto& body : bodies) {
        Files files; files.write("invalid.console", std::string(prefix) + body);
        auto config = files.config("invalid.console"); config.script_root = files.root;
        Scenarios::Job job(config);
        EXPECT_NE(batch(job), 0); EXPECT_EQ(job.disposition(), Script::Disposition::Invalid); ASSERT_NE(job.fixture(), nullptr);
        EXPECT_EQ(image(job).record_count, 0u);
    }
}

TEST(ConsoleScenarios, UnknownFixtureIsInvalidNotAnAliasForHeal) {
    Files files; files.write("unknown.console", "script.version 1\nfixture.load unknown\nfinish\n");
    auto config = files.config("unknown.console"); config.script_root = files.root;
    Scenarios::Job job(config);
    EXPECT_NE(batch(job), 0); EXPECT_EQ(job.disposition(), Script::Disposition::Invalid); EXPECT_EQ(job.fixture(), nullptr);
    EXPECT_NE(job.batch_exit(), 0);
}

TEST(ConsoleScenarios, HeldDeliveryTimesOutWithAppliedEffectAndNoRetry) {
    Files files; Scenarios::Job job(files.config("timeout.console"));
    EXPECT_NE(batch(job), 0); EXPECT_EQ(job.disposition(), Script::Disposition::Incomplete);
    EXPECT_EQ(actor(job, 2).health, 20u); EXPECT_EQ(actor(job, 1).reserve, 7u);
    EXPECT_EQ(image(job).record_count, 1u); EXPECT_FALSE(actor(job, 3).access);
    EXPECT_FALSE(Scenarios::completed_report(files.root, "run", job.source_id(), "source-tests-not-runtime-evidence"));
}

TEST(ConsoleScenarios, IndependentFixturePricesPrincipalsAndSessions) {
    Files files;
    Scenarios::Job first(files.config("heal.console", "first"));
    Scenarios::Job second(files.config("heal.console", "second"));
    ASSERT_TRUE(initialize(first)); ASSERT_TRUE(initialize(second));
    ASSERT_NE(first.fixture(), nullptr); ASSERT_NE(second.fixture(), nullptr);
    ASSERT_TRUE(first.fixture()->host->set_price(9));
    EXPECT_EQ(second.fixture()->host->price(), 3u);
    EXPECT_FALSE(first.fixture()->host->actor(second.fixture()->access, first.fixture()->host->entity(1)));
    EXPECT_EQ(batch(second), 0); EXPECT_EQ(actor(first, 2).health, 12u);
    first.close(); EXPECT_EQ(first.disposition(), Script::Disposition::Cancelled);
}

TEST(ConsoleScenarios, ClosePendingUsesFinitePolicyAndRetainsAppliedEffects) {
    Files files; Scenarios::Job job(files.config("timeout.console"));
    ASSERT_TRUE(applied(job));
    ASSERT_EQ(image(job).record_count, 1u);
    job.close();
    EXPECT_EQ(job.disposition(), Script::Disposition::Cancelled);
    EXPECT_EQ(actor(job, 2).health, 20u); EXPECT_EQ(actor(job, 1).reserve, 7u);
    EXPECT_EQ(image(job).record_count, 1u); EXPECT_NE(job.batch_exit(), 0);
}

TEST(ConsoleScenarios, PathsSymlinksAndCollisionFailBeforeEffects) {
    Files files;
    auto traversal = files.config("../heal.console");
    Scenarios::Job outside(traversal); EXPECT_NE(batch(outside), 0); EXPECT_EQ(outside.fixture(), nullptr);
    auto output_traversal = files.config("heal.console", "../escape");
    Scenarios::Job escape(output_traversal); EXPECT_NE(batch(escape), 0); EXPECT_EQ(escape.fixture(), nullptr);
    std::filesystem::create_symlink(std::filesystem::path(SCENARIO_FIXTURES) / "heal.console", files.root / "link.console");
    auto link_config = files.config("link.console"); link_config.script_root = files.root;
    Scenarios::Job linked(link_config); EXPECT_NE(batch(linked), 0); EXPECT_EQ(linked.fixture(), nullptr);
    std::filesystem::create_directory(files.root / "occupied");
    files.write("occupied/keep", "untouched");
    Scenarios::Job collision(files.config("heal.console", "occupied"));
    EXPECT_NE(batch(collision), 0);
    EXPECT_EQ(collision.fixture(), nullptr); EXPECT_EQ(Scenarios::read_source(files.root, "occupied/keep"), "untouched");
    std::filesystem::create_directory(files.root / "directory.console");
    auto directory_config = files.config("directory.console"); directory_config.script_root = files.root;
    Scenarios::Job directory(directory_config); EXPECT_NE(batch(directory), 0); EXPECT_EQ(directory.fixture(), nullptr);
}

TEST(ConsoleScenarios, OutputCapacityCannotPassAndSinkFailureKeepsEffects) {
    Files files;
    std::string body = std::string(prefix) + "magic.cast S1 marin -> cast\nawait $cast max_steps=4 -> result\n";
    for (unsigned assertion = 0; assertion < 30; ++assertion) body += "expect $result.gain == 8\n";
    body += "finish\n"; files.write("capacity.console", body);
    auto config = files.config("capacity.console"); config.script_root = files.root; config.output_capacity = 12000;
    Scenarios::Job job(config);
    EXPECT_NE(batch(job), 0); EXPECT_EQ(job.disposition(), Script::Disposition::Incomplete); EXPECT_FALSE(job.durable());
    ASSERT_NE(job.fixture(), nullptr); EXPECT_EQ(actor(job, 2).health, 20u); EXPECT_EQ(actor(job, 1).reserve, 7u);
    EXPECT_EQ(image(job).record_count, 1u); EXPECT_FALSE(std::filesystem::exists(files.root / "run/complete"));
    auto tiny = files.config("heal.console", "tiny"); tiny.output_capacity = 1024;
    Scenarios::Job no_capacity(tiny); EXPECT_NE(batch(no_capacity), 0);
}

TEST(ConsoleScenarios, MissingFinalMarkerAndStaleSourceNeverPass) {
    Files files; Scenarios::Job job(files.config("heal.console")); ASSERT_EQ(batch(job), 0);
    EXPECT_FALSE(Scenarios::completed_report(files.root, "run", "stale-source", "source-tests-not-runtime-evidence"));
    EXPECT_FALSE(Scenarios::completed_report(files.root, "run", job.source_id(), "stale-build"));
    std::filesystem::remove(files.root / "run/complete");
    EXPECT_FALSE(Scenarios::completed_report(files.root, "run", job.source_id(), "source-tests-not-runtime-evidence"));
    Scenarios::Job collision(files.config("heal.console")); EXPECT_NE(batch(collision), 0);
}

TEST(ConsoleScenarios, TruncatedTranscriptCannotSatisfyCompletionMarker) {
    Files files; Scenarios::Job job(files.config("heal.console")); ASSERT_EQ(batch(job), 0);
    files.write("run/transcript.txt", "truncated");
    EXPECT_FALSE(Scenarios::completed_report(files.root, "run", job.source_id(), "source-tests-not-runtime-evidence"));
}

TEST(ConsoleScenarios, InteractiveRegistrationUsesSameRunnerAndStopsAdmission) {
    Files files; auto practice = Living::Console::practice();
    auto observer = practice.console->open(Living::Console::Mode::PracticeObserver);
    auto gameplay = practice.console->open(Living::Console::Mode::Gameplay);
    Scenarios::Commands commands(practice.console->registry(), files.config(""));
    EXPECT_FALSE(Core::bind_text(practice.console->registry(), gameplay->context(), "test.run heal.console interactive"));
    for (const auto& descriptor : practice.console->registry().commands(gameplay->context())) EXPECT_NE(descriptor.name, "test.run");
    auto suggestions = Core::complete(practice.console->registry(), gameplay->context(), "test.");
    ASSERT_TRUE(suggestions); EXPECT_TRUE(suggestions->empty());
    auto request = Core::bind_text(practice.console->registry(), observer->context(), "test.run heal.console interactive");
    ASSERT_TRUE(request); ASSERT_TRUE(observer->session().submit(1, *request));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        ASSERT_TRUE(observer->session().pump());
        auto retained = observer->session().lookup(1); ASSERT_TRUE(retained);
        if (Core::terminal((**retained).stage)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    auto retained = observer->session().lookup(1); ASSERT_TRUE(retained);
    EXPECT_EQ((**retained).stage, Core::Stage::Succeeded);
    const auto result = **retained;
    EXPECT_EQ(std::get<std::string>(std::get<Core::Value::Children>(result.value.data)[0].data), "passed");
    commands.close();
    EXPECT_FALSE(Core::bind_text(practice.console->registry(), observer->context(), "test.run heal.console another"));
    EXPECT_EQ(Living::HostTestAccess::image(*practice.host).record_count, 0u);
}

TEST(ConsoleScenarios, ClosingQueuedRegistrationDoesNotAdoptReplacement) {
    Files files; auto practice = Living::Console::practice();
    auto observer = practice.console->open(Living::Console::Mode::PracticeObserver);
    Scenarios::Commands old(practice.console->registry(), files.config(""));
    auto bound = Core::bind_text(practice.console->registry(), observer->context(), "test.run heal.console stale").value();
    ASSERT_TRUE(observer->session().submit(1, bound)); old.close();
    Scenarios::Commands replacement(practice.console->registry(), files.config(""));
    ASSERT_TRUE(observer->session().pump());
    auto retained = observer->session().lookup(1); ASSERT_TRUE(retained);
    EXPECT_EQ((**retained).stage, Core::Stage::Cancelled);
    EXPECT_FALSE(std::filesystem::exists(files.root / "stale"));
}

TEST(ConsoleScenarios, InteractiveWrongAssertionReturnsFailedJobWithoutFailingSession) {
    Files files; auto practice = Living::Console::practice();
    auto observer = practice.console->open(Living::Console::Mode::PracticeObserver);
    Scenarios::Commands commands(practice.console->registry(), files.config(""));
    auto request = Core::bind_text(practice.console->registry(), observer->context(),
        "test.run wrong-assertion.console negative");
    ASSERT_TRUE(request); ASSERT_TRUE(observer->session().submit(1, *request));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        ASSERT_TRUE(observer->session().pump());
        auto retained = observer->session().lookup(1); ASSERT_TRUE(retained);
        if (Core::terminal((**retained).stage)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    auto retained = observer->session().lookup(1); ASSERT_TRUE(retained);
    ASSERT_EQ((**retained).stage, Core::Stage::Succeeded);
    const auto result = **retained;
    const auto& fields = std::get<Core::Value::Children>(result.value.data);
    EXPECT_EQ(std::get<std::string>(fields[0].data), "failed");
    EXPECT_TRUE(std::get<bool>(fields[2].data));
    EXPECT_EQ(Living::HostTestAccess::image(*practice.host).record_count, 0u);
    EXPECT_TRUE(Core::bind_text(practice.console->registry(), observer->context(), "actor.read marin"));
}

TEST(ConsoleScenarios, ClosingInteractiveGenerationFinishesPendingJobWithoutExit) {
    Files files; auto practice = Living::Console::practice();
    auto observer = practice.console->open(Living::Console::Mode::PracticeObserver);
    Scenarios::Commands commands(practice.console->registry(), files.config(""));
    auto request = Core::bind_text(practice.console->registry(), observer->context(), "test.run timeout.console closing").value();
    ASSERT_TRUE(observer->session().submit(1, request)); ASSERT_TRUE(observer->session().pump());
    commands.close();
    auto retained = observer->session().lookup(1); ASSERT_TRUE(retained);
    EXPECT_TRUE(Core::terminal((**retained).stage));
    EXPECT_NE(std::get<std::string>(std::get<Core::Value::Children>((**retained).value.data)[0].data), "passed");
    EXPECT_FALSE(Scenarios::completed_report(files.root, "closing", "not-a-completed-source", "source-tests-not-runtime-evidence"));
}
#else
TEST(ConsoleScenarios, UnsupportedFileAdapterFailsClosedBeforeFixtureCreation) {
    Files files; Scenarios::Job job(files.config("heal.console"));
    EXPECT_NE(batch(job), 0);
    EXPECT_EQ(job.disposition(), Script::Disposition::Invalid);
    EXPECT_NE(job.batch_exit(), 0);
    EXPECT_EQ(job.fixture(), nullptr);
    EXPECT_FALSE(job.durable());
}
#endif
