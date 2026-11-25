#include <tools/scenarios/command.h>
#include <tests/host/access.h>

#include <gtest/gtest.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace {
    namespace Living = CE::LivingWorld;
    namespace Scenarios = Living::Scenarios;
    namespace Core = CE::Tools::Console;
    namespace Script = Core::Scenario;

    class Gate {
        std::mutex mutex_;
        std::condition_variable wake_;
        Scenarios::IoOperation operation_ = Scenarios::IoOperation::None;
        bool entered_ = false, released_ = true, fail_ = false;
    public:
        void arm(Scenarios::IoOperation operation, bool fail = false) {
            std::lock_guard lock(mutex_);
            operation_ = operation; entered_ = false; released_ = false; fail_ = fail;
        }
        void enter(Scenarios::IoOperation operation) {
            std::unique_lock lock(mutex_);
            if (operation != operation_) return;
            entered_ = true; wake_.notify_all();
            wake_.wait(lock, [&] { return released_; });
            if (fail_) throw std::runtime_error("Controlled I/O failure");
        }
        bool entered() {
            std::lock_guard lock(mutex_); return entered_;
        }
        bool wait() {
            std::unique_lock lock(mutex_);
            return wake_.wait_for(lock, std::chrono::seconds(5), [&] { return entered_; });
        }
        void release() {
            std::lock_guard lock(mutex_); released_ = true; wake_.notify_all();
        }
    };
    class HostIo {
        static inline std::atomic<unsigned> sequence = 0;
    public:
        CE::Workers::Pool pool{1};
        CE::Workers::Group group = pool.make_group();
        std::shared_ptr<Gate> gate = std::make_shared<Gate>();
        std::shared_ptr<Scenarios::IoService> io;
        std::filesystem::path root = std::filesystem::temp_directory_path() /
            ("cheryl-io-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" + std::to_string(++sequence));
        explicit HostIo(std::size_t bytes = Scenarios::IoService::max_queued_bytes)
            : io(std::make_shared<Scenarios::IoService>(group,
                [owned = gate](auto operation) { owned->enter(operation); }, bytes)) {
            if (!std::filesystem::create_directory(root)) throw std::runtime_error("Cannot create I/O test root");
        }
        ~HostIo() {
            gate->release(); io->settle();
            std::error_code ignored; std::filesystem::remove_all(root, ignored);
        }
        Scenarios::Config config(std::string run = "run", std::string script = "heal.console") const {
            Scenarios::Config config;
            config.script_root = SCENARIO_FIXTURES; config.asset_root = SCENARIO_ASSETS; config.output_root = root;
            config.script = std::move(script); config.run_id = std::move(run); config.build_id = "io-source-regressions"; config.io = io;
            return config;
        }
        Scenarios::IoInput input(std::string run) const {
            const auto config = this->config(std::move(run));
            return {config.script_root, config.asset_root, config.output_root, config.script, config.run_id, config.build_id,
                config.output_capacity, config.asset};
        }
        void settle() { gate->release(); io->settle(); pool.shutdown(); }
    };
    template<class Predicate> bool until(Predicate predicate) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!predicate()) {
            if (std::chrono::steady_clock::now() >= deadline) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return true;
    }
    bool terminal(Scenarios::Job& job) {
        return until([&] { return job.step() != Script::Disposition::Running; });
    }
    bool applied(Scenarios::Job& job) {
        return until([&] {
            job.step();
            return job.fixture() && Living::HostTestAccess::image(*job.fixture()->host).record_count == 1;
        });
    }
}

#if defined(__unix__) || defined(__APPLE__)
TEST(ConsoleScenarios, InputRetiresWithoutOwnerOrPoolWait) {
    HostIo host; host.gate->arm(Scenarios::IoOperation::Input);
    auto job = std::make_unique<Scenarios::Job>(host.config());
    ASSERT_TRUE(host.gate->wait());
    EXPECT_EQ(job->step(), Script::Disposition::Running); EXPECT_EQ(job->fixture(), nullptr);
    job->close(); EXPECT_EQ(job->disposition(), Script::Disposition::Cancelled);
    job.reset();
    EXPECT_EQ(host.io->retained(), 1u); EXPECT_FALSE(host.io->settled());
    host.io->shutdown(); EXPECT_FALSE(host.io->settled());
    host.settle();
    EXPECT_TRUE(host.io->settled()); EXPECT_EQ(host.io->retained(), 0u);
    EXPECT_FALSE(std::filesystem::exists(host.root / "run/complete"));
}

TEST(ConsoleScenarios, AdmissionIsBoundedWhileInputIsHeld) {
    HostIo host; host.gate->arm(Scenarios::IoOperation::Input);
    std::vector<Scenarios::IoHandle> handles;
    for (std::size_t index = 0; index < Scenarios::IoService::max_jobs; ++index) {
        auto handle = host.io->admit(host.input("run-" + std::to_string(index)));
        ASSERT_TRUE(handle); handles.push_back(std::move(handle));
    }
    ASSERT_TRUE(host.gate->wait());
    EXPECT_FALSE(host.io->admit(host.input("overflow")));
    for (auto& handle : handles) handle.close();
    EXPECT_EQ(host.io->retained(), Scenarios::IoService::max_jobs);
    host.settle(); EXPECT_EQ(host.io->retained(), 0u);
}

TEST(ConsoleScenarios, ByteBackpressurePrecedesSnapshotPublication) {
    HostIo host(2 * Script::max_file_bytes);
    auto first = host.io->admit(host.input("first"));
    ASSERT_TRUE(first);
    ASSERT_TRUE(until([&] { return first.poll().status == Scenarios::IoStatus::Input; }));
    host.gate->arm(Scenarios::IoOperation::Input);
    auto held = host.io->admit(host.input("held")); ASSERT_TRUE(held);
    ASSERT_TRUE(host.gate->wait());
    const std::string oversized(Script::max_file_bytes + 1, 'x');
    EXPECT_EQ(first.submit(oversized, false, "source"), Scenarios::IoSubmit::Full);
    EXPECT_EQ(first.submit("{}", false, "source"), Scenarios::IoSubmit::Full);
    host.gate->release();
    ASSERT_TRUE(until([&] { return held.poll().status == Scenarios::IoStatus::Input; }));
    EXPECT_EQ(first.submit("{}", false, "source"), Scenarios::IoSubmit::Accepted);
    EXPECT_EQ(first.submit("{}", false, "source"), Scenarios::IoSubmit::Busy);
    first.close(); held.close();
    host.settle();
}

TEST(ConsoleScenarios, RunningOutputAndCleanupDoNotOwnTheInteractiveLifetime) {
    HostIo host;
    auto job = std::make_unique<Scenarios::Job>(host.config());
    ASSERT_TRUE(applied(*job));
    host.gate->arm(Scenarios::IoOperation::Snapshot);
    ASSERT_TRUE(until([&] { job->step(); return host.gate->entered(); }));
    EXPECT_EQ(job->step(), Script::Disposition::Running);
    const auto fixture = *job->fixture();
    job->close(); job.reset();
    EXPECT_EQ(host.io->retained(), 1u);
    EXPECT_EQ(fixture.host->actor(fixture.access, fixture.host->entity(2))->health, 20u);
    EXPECT_EQ(fixture.host->actor(fixture.access, fixture.host->entity(1))->reserve, 7u);
    EXPECT_EQ(Living::HostTestAccess::image(*fixture.host).record_count, 1u);
    host.gate->release();
    ASSERT_TRUE(until([&] { return host.io->retained() == 0; }));
    host.gate->arm(Scenarios::IoOperation::Cleanup);
    auto pending = std::make_unique<Scenarios::Job>(host.config("cleanup"));
    pending->close(); pending.reset(); ASSERT_TRUE(host.gate->wait());
    EXPECT_EQ(host.io->retained(), 1u); EXPECT_FALSE(host.io->settled());
    host.settle(); EXPECT_EQ(host.io->retained(), 0u);
}

TEST(ConsoleScenarios, SinkFailureRetainsTheRealHealAndDebit) {
    HostIo host; Scenarios::Job job(host.config());
    ASSERT_TRUE(applied(job));
    host.gate->arm(Scenarios::IoOperation::Snapshot, true);
    ASSERT_TRUE(until([&] { job.step(); return host.gate->entered(); }));
    EXPECT_EQ(job.step(), Script::Disposition::Running);
    host.gate->release(); ASSERT_TRUE(terminal(job));
    EXPECT_EQ(job.disposition(), Script::Disposition::Incomplete); EXPECT_FALSE(job.durable()); EXPECT_NE(job.batch_exit(), 0);
    EXPECT_EQ(job.fixture()->host->actor(job.fixture()->access, job.fixture()->host->entity(2))->health, 20u);
    EXPECT_EQ(job.fixture()->host->actor(job.fixture()->access, job.fixture()->host->entity(1))->reserve, 7u);
    EXPECT_EQ(Living::HostTestAccess::image(*job.fixture()->host).record_count, 1u);
    host.settle(); EXPECT_FALSE(std::filesystem::exists(host.root / "run/complete"));
}

TEST(ConsoleScenarios, CloseWinsOverHeldFinalization) {
    HostIo host; host.gate->arm(Scenarios::IoOperation::Finalize);
    Scenarios::Job job(host.config());
    ASSERT_TRUE(until([&] { job.step(); return host.gate->entered(); }));
    EXPECT_EQ(job.disposition(), Script::Disposition::Running); EXPECT_FALSE(job.durable());
    job.close(); EXPECT_EQ(job.disposition(), Script::Disposition::Cancelled); EXPECT_NE(job.batch_exit(), 0);
    EXPECT_EQ(host.io->retained(), 1u);
    host.settle();
    EXPECT_EQ(job.step(), Script::Disposition::Cancelled); EXPECT_FALSE(job.durable());
    EXPECT_FALSE(std::filesystem::exists(host.root / "run/complete"));
}

TEST(ConsoleScenarios, CloseAfterFsyncRemovesUncommittedMarker) {
    HostIo host; host.gate->arm(Scenarios::IoOperation::Publish);
    auto job = std::make_unique<Scenarios::Job>(host.config());
    ASSERT_TRUE(until([&] { job->step(); return host.gate->entered(); }));
    ASSERT_TRUE(std::filesystem::exists(host.root / "run/complete"));
    EXPECT_FALSE(job->durable()); job->close();
    EXPECT_EQ(job->disposition(), Script::Disposition::Cancelled); EXPECT_NE(job->batch_exit(), 0);
    job.reset(); EXPECT_EQ(host.io->retained(), 1u);
    host.settle(); EXPECT_FALSE(std::filesystem::exists(host.root / "run/complete"));
}

TEST(ConsoleScenarios, CommittedFinalizationWinsOverClose) {
    HostIo host; Scenarios::Job job(host.config());
    ASSERT_TRUE(terminal(job)); ASSERT_EQ(job.batch_exit(), 0);
    job.close(); EXPECT_EQ(job.disposition(), Script::Disposition::Passed); EXPECT_TRUE(job.durable());
    host.settle();
    EXPECT_TRUE(Scenarios::completed_report(host.root, "run", job.source_id(), "io-source-regressions"));
}

TEST(ConsoleScenarios, OldInputCannotPopulateReplacementGeneration) {
    HostIo host; host.gate->arm(Scenarios::IoOperation::Input);
    auto old = std::make_unique<Scenarios::Job>(host.config("old"));
    ASSERT_TRUE(host.gate->wait()); old->close(); old.reset();
    Scenarios::Job replacement(host.config("replacement", "refused.console"));
    EXPECT_EQ(replacement.step(), Script::Disposition::Running); EXPECT_EQ(replacement.fixture(), nullptr);
    EXPECT_EQ(host.io->retained(), 2u);
    host.gate->release(); ASSERT_TRUE(terminal(replacement));
    ASSERT_EQ(replacement.batch_exit(), 0);
    EXPECT_EQ(replacement.fixture()->host->actor(replacement.fixture()->access, replacement.fixture()->host->entity(1))->reserve, 2u);
    EXPECT_EQ(replacement.fixture()->host->actor(replacement.fixture()->access, replacement.fixture()->host->entity(2))->health, 12u);
    host.settle(); EXPECT_FALSE(std::filesystem::exists(host.root / "old/complete"));
    EXPECT_TRUE(Scenarios::completed_report(host.root, "replacement", replacement.source_id(), "io-source-regressions"));
}

TEST(ConsoleScenarios, CommandsCloseWhileInputIsHeld) {
    HostIo host; host.gate->arm(Scenarios::IoOperation::Input);
    auto practice = Living::Console::practice();
    auto observer = practice.console->open(Living::Console::Mode::PracticeObserver);
    auto commands = std::make_unique<Scenarios::Commands>(practice.console->registry(), host.config());
    auto request = Core::bind_text(practice.console->registry(), observer->context(), "test.run heal.console command");
    ASSERT_TRUE(request); ASSERT_TRUE(observer->session().submit(1, *request));
    ASSERT_TRUE(observer->session().pump()); ASSERT_TRUE(host.gate->wait());
    commands->close(); commands.reset();
    const auto retained = observer->session().lookup(1); ASSERT_TRUE(retained);
    EXPECT_TRUE(Core::terminal((**retained).stage));
    EXPECT_EQ(host.io->retained(), 1u); EXPECT_FALSE(host.io->settled());
    observer.reset(); practice.console.reset();
    host.settle(); EXPECT_FALSE(std::filesystem::exists(host.root / "command/complete"));
}

TEST(ConsoleScenarios, RetiredHandleCannotCloseReusedSlot) {
    HostIo host; host.gate->arm(Scenarios::IoOperation::Input);
    auto old = host.io->admit(host.input("old")); ASSERT_TRUE(old);
    ASSERT_TRUE(host.gate->wait()); old.close();
    host.gate->release(); ASSERT_TRUE(until([&] { return host.io->retained() == 0; }));
    auto replacement = host.io->admit(host.input("replacement")); ASSERT_TRUE(replacement);
    EXPECT_EQ(old.poll().status, Scenarios::IoStatus::Cancelled);
    EXPECT_EQ(old.submit("{}", false, "old"), Scenarios::IoSubmit::Closed);
    EXPECT_EQ(old.close(), Scenarios::IoStatus::Cancelled);
    ASSERT_TRUE(until([&] { return replacement.poll().status == Scenarios::IoStatus::Input; }));
    EXPECT_EQ(host.io->retained(), 1u);
    replacement.close(); host.settle();
    EXPECT_FALSE(std::filesystem::exists(host.root / "old/complete"));
}

TEST(ConsoleScenarios, CommittedReceiptSurvivesSlotRetirement) {
    HostIo host;
    auto completed = host.io->admit(host.input("completed")); ASSERT_TRUE(completed);
    ASSERT_TRUE(until([&] { return completed.poll().status == Scenarios::IoStatus::Input; }));
    ASSERT_EQ(completed.submit("{\"schema\":1,\"final\":true,\"disposition\":\"passed\",\"results\":[]}", true, "source"), Scenarios::IoSubmit::Accepted);
    ASSERT_TRUE(until([&] { return completed.poll().status == Scenarios::IoStatus::Complete; }));
    EXPECT_EQ(completed.close(), Scenarios::IoStatus::Complete);
    ASSERT_TRUE(until([&] { return host.io->retained() == 0; }));
    auto replacement = host.io->admit(host.input("replacement")); ASSERT_TRUE(replacement);
    EXPECT_EQ(completed.poll().status, Scenarios::IoStatus::Complete);
    EXPECT_EQ(completed.close(), Scenarios::IoStatus::Complete);
    ASSERT_TRUE(until([&] { return replacement.poll().status == Scenarios::IoStatus::Input; }));
    host.settle();
    EXPECT_TRUE(Scenarios::completed_report(host.root, "completed", "source", "io-source-regressions"));
    EXPECT_EQ(completed.poll().status, Scenarios::IoStatus::Complete);
}

TEST(ConsoleScenarios, ClosedGroupRejectsTheLaneBeforeAnyInput) {
    HostIo host; auto group = host.pool.make_group(); group.close();
    EXPECT_THROW(static_cast<void>(Scenarios::IoService(group)), std::exception);
    EXPECT_EQ(host.io->retained(), 0u);
    host.settle(); EXPECT_TRUE(std::filesystem::is_empty(host.root));
}

TEST(ConsoleScenarios, GroupClosureCannotStrandAcceptedCleanup) {
    HostIo host; host.gate->arm(Scenarios::IoOperation::Input);
    auto job = std::make_unique<Scenarios::Job>(host.config());
    ASSERT_TRUE(host.gate->wait()); host.group.close();
    job->close(); job.reset();
    EXPECT_EQ(host.io->retained(), 1u); EXPECT_EQ(host.group.status().accepted, 1u);
    host.settle();
    EXPECT_EQ(host.io->retained(), 0u); EXPECT_EQ(host.group.status().accepted, 1u);
    EXPECT_FALSE(std::filesystem::exists(host.root / "run/complete"));
}
#endif
