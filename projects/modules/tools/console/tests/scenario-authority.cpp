#include <cheryl/tools/console/scenario.h>
#include <gtest/gtest.h>
#include <new>
#include <stdexcept>

namespace {
    namespace Core = CE::Tools::Console;
    namespace Scenario = Core::Scenario;

    class AmountWork final : public Core::Work {
        std::uint64_t& effects_;
        std::size_t& progress_;
        std::uint64_t amount_;
        std::size_t remaining_;
        bool applies_;
    public:
        AmountWork(std::uint64_t& effects, std::size_t& progress, std::uint64_t amount,
                   std::size_t remaining, bool applies)
            : effects_(effects), progress_(progress), amount_(amount), remaining_(remaining), applies_(applies) {
            result.value = Core::Value::unsigned_integer(amount);
        }
        void step() noexcept override {
            if (Core::terminal(result.stage)) return;
            ++progress_;
            if (--remaining_) { result.stage = Core::Stage::Pending; return; }
            if (applies_) effects_ += amount_;
            result.stage = Core::Stage::Succeeded;
        }
        void close() noexcept override {
            if (!Core::terminal(result.stage)) {
                result.stage = Core::Stage::Cancelled; result.failure = {Core::Code::Cancelled};
            }
        }
    };
    class SnapshotWork final : public Core::Work {
        Core::Stage stage_;
    public:
        explicit SnapshotWork(Core::Value value, Core::Stage stage = Core::Stage::Succeeded) : stage_(stage) {
            result.value = std::move(value);
        }
        void step() noexcept override { if (!Core::terminal(result.stage)) result.stage = stage_; }
        void close() noexcept override {
            if (!Core::terminal(result.stage)) {
                result.stage = Core::Stage::Cancelled; result.failure = {Core::Code::Cancelled};
            }
        }
    };
    class DeferredFailureWork final : public Core::Work {
        bool entered_ = false;
    public:
        DeferredFailureWork() { result.value = Core::Value::unsigned_integer(0); }
        void step() noexcept override {
            if (Core::terminal(result.stage)) return;
            if (!entered_) { entered_ = true; result.stage = Core::Stage::Pending; }
            else { result.stage = Core::Stage::Failed; result.failure = {Core::Code::Range, 7, 2}; }
        }
        void close() noexcept override {
            if (!Core::terminal(result.stage)) {
                result.stage = Core::Stage::Cancelled; result.failure = {Core::Code::Cancelled};
            }
        }
    };
    class OperationWork final : public Core::Work {
        Core::Authority& authority_;
        std::size_t& minted_;
        std::uint64_t& effects_;
        bool applies_;
    public:
        OperationWork(Core::Authority& authority, std::size_t& minted, std::uint64_t& effects, bool applies)
            : authority_(authority), minted_(minted), effects_(effects), applies_(applies) {}
        void step() noexcept override {
            if (Core::terminal(result.stage)) return;
            if (result.stage == Core::Stage::Pending) { result.stage = Core::Stage::Succeeded; return; }
            try {
                auto operation = authority_.mint("operation");
                if (!operation) { result.stage = Core::Stage::Failed; result.failure = operation.error(); return; }
                result.value = Core::Value::record({Core::Value::record({Core::Value::handle(*operation)})});
                ++minted_;
                if (applies_) ++effects_;
                result.stage = Core::Stage::Pending;
            } catch (const std::bad_alloc&) { result.stage = Core::Stage::Failed; result.failure = {Core::Code::Allocation}; }
        }
        void close() noexcept override {
            if (!Core::terminal(result.stage)) {
                result.stage = Core::Stage::Cancelled; result.failure = {Core::Code::Cancelled};
            }
        }
    };
    class ConsoleScenarioAuthority : public testing::Test {
    protected:
        Core::Authority gameplay_authority_, diagnostic_authority_;
        Core::Context gameplay_ = gameplay_authority_.issue(1), diagnostic_ = diagnostic_authority_.issue(7);
        Core::Session gameplay_session_{gameplay_}, diagnostic_session_{diagnostic_};
        Scenario::Channels channels_{{gameplay_, gameplay_session_}, Scenario::Channel{diagnostic_, diagnostic_session_}};
        bool gameplay_accepts_ = true, diagnostic_accepts_ = true;
        Core::Registry registry_{[this](const Core::Context& context) {
            return (gameplay_accepts_ && gameplay_.same_scope(context)) ||
                (diagnostic_accepts_ && diagnostic_.same_scope(context));
        }};
        std::vector<Core::Registration> registrations_;
        std::uint64_t effects_ = 0;
        std::size_t gameplay_progress_ = 0, diagnostic_progress_ = 0;
        std::size_t amount_entries_ = 0, diagnostic_entries_ = 0;
        Core::Schema integer_, handle_, nested_;

        Core::Descriptor descriptor(std::string name, Core::Rights rights, Core::Kind kind,
                                    Core::Schema result, std::vector<Core::Argument> arguments = {}) {
            Core::Descriptor value;
            value.name = std::move(name); value.domain = "counter"; value.rights = rights;
            value.kind = kind; value.result = std::move(result); value.arguments = std::move(arguments);
            return value;
        }
        bool amount(std::string name, Core::Kind kind, Core::Rights rights, std::size_t delay) {
            auto installed = registry_.add<std::uint64_t>(descriptor(std::move(name), rights, kind, integer_, {{"amount", integer_}}),
                [this, delay](const Core::Context&, std::uint64_t amount) {
                    ++amount_entries_;
                    return std::make_shared<AmountWork>(effects_, gameplay_progress_, amount, delay, true);
                });
            if (!installed) return false;
            registrations_.push_back(std::move(*installed)); return true;
        }
        bool snapshot(std::string name, Core::Rights rights, Core::Schema schema, Core::Stage stage) {
            auto installed = registry_.add<>(descriptor(std::move(name), rights, Core::Kind::Query, schema),
                [this, schema, stage](const Core::Context& context) {
                    if (schema.type == Core::Type::Unsigned)
                        return std::make_shared<SnapshotWork>(Core::Value::unsigned_integer(effects_), stage);
                    auto handle = context.resolve("actor", "actor");
                    if (!handle) throw std::runtime_error("Missing fixture actor");
                    auto value = Core::Value::handle(*handle);
                    if (schema.type == Core::Type::Record) value = Core::Value::record({Core::Value::record({value})});
                    return std::make_shared<SnapshotWork>(std::move(value), stage);
                });
            if (!installed) return false;
            registrations_.push_back(std::move(*installed)); return true;
        }
        void SetUp() override {
            integer_.type = Core::Type::Unsigned;
            handle_.type = Core::Type::Handle; handle_.domain = "actor";
            Core::Schema inner; inner.type = Core::Type::Record; inner.fields = {"actor"}; inner.children = {handle_};
            nested_.type = Core::Type::Record; nested_.fields = {"nested"}; nested_.children = {inner};
            auto gameplay_handle = gameplay_authority_.mint("actor"); ASSERT_TRUE(gameplay_handle);
            auto diagnostic_handle = diagnostic_authority_.mint("actor"); ASSERT_TRUE(diagnostic_handle);
            ASSERT_EQ(gameplay_handle->key(), diagnostic_handle->key());
            ASSERT_NE(*gameplay_handle, *diagnostic_handle);
            ASSERT_TRUE(gameplay_authority_.alias(gameplay_, "actor", *gameplay_handle));
            ASSERT_TRUE(diagnostic_authority_.alias(diagnostic_, "actor", *diagnostic_handle));
            ASSERT_TRUE(amount("cast", Core::Kind::Action, 1, 1));
            ASSERT_TRUE(amount("slow", Core::Kind::Action, 1, 3));
            ASSERT_TRUE(amount("setting", Core::Kind::Setting, 4, 1));
            ASSERT_TRUE(snapshot("read", 2, integer_, Core::Stage::Succeeded));
            ASSERT_TRUE(snapshot("local", 1, integer_, Core::Stage::Succeeded));
            ASSERT_TRUE(snapshot("handle", 1, handle_, Core::Stage::Succeeded));
            ASSERT_TRUE(snapshot("bundle", 1, nested_, Core::Stage::Succeeded));
            ASSERT_TRUE(snapshot("reject", 1, integer_, Core::Stage::Refused));
            auto delayed = registry_.add<>(descriptor("watch", 2, Core::Kind::Query, integer_), [this](const Core::Context&) {
                ++diagnostic_entries_;
                return std::make_shared<AmountWork>(effects_, diagnostic_progress_, 41, 3, false);
            });
            ASSERT_TRUE(delayed); registrations_.push_back(std::move(*delayed));
            auto echo = registry_.add<std::uint64_t>(descriptor("echo", 2, Core::Kind::Query, integer_, {{"amount", integer_}}),
                [this](const Core::Context&, std::uint64_t amount) {
                    ++diagnostic_entries_;
                    return std::make_shared<AmountWork>(effects_, diagnostic_progress_, amount, 1, false);
                });
            ASSERT_TRUE(echo); registrations_.push_back(std::move(*echo));
            auto consume = registry_.add<Core::Handle>(descriptor("consume", 1, Core::Kind::Query, integer_, {{"actor", handle_}}),
                [](const Core::Context&, Core::Handle actor) {
                    return std::make_shared<SnapshotWork>(Core::Value::unsigned_integer(actor.key()));
                });
            ASSERT_TRUE(consume); registrations_.push_back(std::move(*consume));
            auto broken = registry_.add<>(descriptor("broken", 1, Core::Kind::Query, integer_),
                [](const Core::Context&) -> std::shared_ptr<Core::Work> { throw std::runtime_error("Owner failure"); });
            ASSERT_TRUE(broken); registrations_.push_back(std::move(*broken));
        }
        Scenario::Checked<Scenario::Plan> prepare(std::string_view body) {
            return Scenario::prepare(registry_, channels_, std::string("script.version 1\nfixture.load counter\n") + std::string(body));
        }
        Scenario::Disposition finish(Scenario::Run& run) {
            for (std::size_t step = 0; step < Scenario::max_statements + Scenario::max_steps + 1; ++step)
                if (run.step() != Scenario::Disposition::Running) return run.disposition();
            return Scenario::Disposition::Running;
        }
    };
}

TEST_F(ConsoleScenarioAuthority, UsesSameHandlerExactlyOnce) {
    auto typed = registry_.bind("cast", {Core::Value::unsigned_integer(3)}, gameplay_); ASSERT_TRUE(typed);
    Core::Session typed_session(gameplay_);
    ASSERT_TRUE(typed_session.submit(1, *typed)); ASSERT_TRUE(typed_session.pump());
    auto typed_result = typed_session.lookup(1); ASSERT_TRUE(typed_result);
    ASSERT_EQ((**typed_result).stage, Core::Stage::Succeeded);
    EXPECT_EQ((**typed_result).value, Core::Value::unsigned_integer(3));
    ASSERT_TRUE(typed_session.submit(1, *typed)); ASSERT_TRUE(typed_session.pump());
    EXPECT_EQ(effects_, 3u);
    auto changed = registry_.bind("cast", {Core::Value::unsigned_integer(4)}, gameplay_); ASSERT_TRUE(changed);
    auto conflict = typed_session.submit(1, *changed); ASSERT_FALSE(conflict);
    EXPECT_EQ(conflict.error().code, Core::Code::Conflict);
    auto text = Core::bind_text(registry_, gameplay_, "cast amount=3"); ASSERT_TRUE(text);
    Core::Session text_session(gameplay_);
    ASSERT_TRUE(text_session.submit(1, *text)); ASSERT_TRUE(text_session.pump());
    auto text_result = text_session.lookup(1); ASSERT_TRUE(text_result);
    EXPECT_EQ((**text_result).stage, Core::Stage::Succeeded);
    EXPECT_EQ((**text_result).value, Core::Value::unsigned_integer(3));
    auto plan = prepare("cast 3 -> casted\nretry $casted -> duplicate\nobserve read -> observed\n"
                        "expect $casted == 3\nexpect $duplicate == 3\nexpect $observed == 9\nfinish");
    ASSERT_TRUE(plan); EXPECT_EQ(effects_, 6u);
    Scenario::Run run(*plan, registry_);
    EXPECT_EQ(finish(run), Scenario::Disposition::Passed);
    EXPECT_EQ(effects_, 9u); EXPECT_EQ(gameplay_progress_, 3u);
    ASSERT_EQ(run.results().size(), 3u);
    EXPECT_EQ(run.results()[0].request_id, 1u); EXPECT_EQ(run.results()[2].request_id, 1u);
    EXPECT_EQ(run.results()[0].route, Scenario::Route::Gameplay);
    EXPECT_EQ(run.results()[2].route, Scenario::Route::Diagnostic);
    EXPECT_EQ(gameplay_session_.retained(), 1u); EXPECT_EQ(diagnostic_session_.retained(), 1u);
}

TEST_F(ConsoleScenarioAuthority, RechecksLiteralGateAtFirstStep) {
    const Core::Failure unauthorized{Core::Code::Unauthorized};
    for (bool diagnostic : {false, true}) {
        gameplay_accepts_ = true; diagnostic_accepts_ = true;
        Core::Session gameplay(gameplay_), observer(diagnostic_);
        Scenario::Channels channels{{gameplay_, gameplay}, Scenario::Channel{diagnostic_, observer}};
        const auto command = diagnostic ? "observe echo 3" : "cast 3";
        ASSERT_TRUE(registry_.bind(diagnostic ? "echo" : "cast", {Core::Value::unsigned_integer(3)},
            diagnostic ? diagnostic_ : gameplay_));
        auto plan = Scenario::prepare(registry_, channels,
            std::string("script.version 1\nfixture.load counter\n") + command +
            " -> first\nexpect $first == 3\nfinish");
        ASSERT_TRUE(plan);
        Scenario::Run run(*plan, registry_);
        EXPECT_EQ(amount_entries_, 0u); EXPECT_EQ(diagnostic_entries_, 0u); EXPECT_EQ(effects_, 0u);
        if (diagnostic) diagnostic_accepts_ = false; else gameplay_accepts_ = false;
        EXPECT_EQ(run.step(), Scenario::Disposition::Incomplete);
        EXPECT_EQ(run.error().failure, unauthorized); EXPECT_EQ(run.error().line, 3u);
        EXPECT_EQ(run.steps(), 0u); EXPECT_EQ(amount_entries_, 0u); EXPECT_EQ(diagnostic_entries_, 0u);
        EXPECT_EQ(gameplay_progress_, 0u); EXPECT_EQ(diagnostic_progress_, 0u); EXPECT_EQ(effects_, 0u);
        EXPECT_EQ(gameplay.retained(), 0u); EXPECT_EQ(observer.retained(), 0u);
        ASSERT_EQ(run.results().size(), 1u);
        EXPECT_EQ(run.results()[0].request_id, 0u); EXPECT_FALSE(run.results()[0].available);
        gameplay_accepts_ = true; diagnostic_accepts_ = true;
        EXPECT_EQ(run.step(), Scenario::Disposition::Incomplete);
        EXPECT_EQ(run.error().failure, unauthorized); EXPECT_EQ(run.error().line, 3u);
        EXPECT_EQ(amount_entries_, 0u); EXPECT_EQ(diagnostic_entries_, 0u);
    }
}

TEST_F(ConsoleScenarioAuthority, RechecksLaterLiteralAndDynamicGates) {
    const Core::Failure unauthorized{Core::Code::Unauthorized};
    for (bool diagnostic : {false, true}) for (bool dynamic : {false, true}) {
        gameplay_accepts_ = true; diagnostic_accepts_ = true;
        Core::Session gameplay(gameplay_), observer(diagnostic_);
        Scenario::Channels channels{{gameplay_, gameplay}, Scenario::Channel{diagnostic_, observer}};
        const auto effects_before = effects_;
        const auto entries_before = amount_entries_;
        ASSERT_TRUE(registry_.bind(diagnostic ? "echo" : "cast", {Core::Value::unsigned_integer(3)},
            diagnostic ? diagnostic_ : gameplay_));
        auto plan = Scenario::prepare(registry_, channels,
            std::string("script.version 1\nfixture.load counter\ncast 3 -> first\n") +
            (diagnostic ? "observe echo " : "cast ") + (dynamic ? "$first" : "3") +
            " -> second\nexpect $second == 3\nfinish");
        ASSERT_TRUE(plan);
        Scenario::Run run(*plan, registry_);
        ASSERT_EQ(run.step(), Scenario::Disposition::Running);
        ASSERT_EQ(effects_, effects_before + 3); ASSERT_EQ(amount_entries_, entries_before + 1);
        ASSERT_EQ(run.results()[0].result.stage, Core::Stage::Succeeded);
        if (diagnostic) diagnostic_accepts_ = false; else gameplay_accepts_ = false;
        const auto expected = dynamic ? Scenario::Disposition::Failed : Scenario::Disposition::Incomplete;
        EXPECT_EQ(run.step(), expected);
        EXPECT_EQ(run.error().failure, unauthorized); EXPECT_EQ(run.error().line, 4u);
        EXPECT_EQ(run.steps(), 1u); EXPECT_EQ(amount_entries_, entries_before + 1);
        EXPECT_EQ(diagnostic_entries_, 0u); EXPECT_EQ(effects_, effects_before + 3);
        EXPECT_EQ(gameplay.retained(), 1u); EXPECT_EQ(observer.retained(), 0u);
        ASSERT_EQ(run.results().size(), 2u);
        EXPECT_TRUE(run.results()[0].available); EXPECT_EQ(run.results()[0].request_id, 1u);
        EXPECT_EQ(run.results()[0].result.value, Core::Value::unsigned_integer(3));
        EXPECT_EQ(run.results()[1].request_id, 0u); EXPECT_FALSE(run.results()[1].available);
        auto retained = gameplay.lookup(1); ASSERT_TRUE(retained);
        EXPECT_EQ((**retained).stage, Core::Stage::Succeeded);
        EXPECT_EQ((**retained).value, Core::Value::unsigned_integer(3));
        gameplay_accepts_ = true; diagnostic_accepts_ = true;
        EXPECT_EQ(run.step(), expected); EXPECT_EQ(effects_, effects_before + 3);
        run.close(Scenario::Disposition::Incomplete);
        EXPECT_EQ(run.error().failure, unauthorized); EXPECT_EQ(run.error().line, 4u);
        EXPECT_EQ(run.results()[0].result.stage, Core::Stage::Succeeded);
    }
}

TEST_F(ConsoleScenarioAuthority, KeepsAdmittedRetryAndAwaitAfterGateCloses) {
    for (bool diagnostic : {false, true}) for (bool pending : {false, true}) {
        gameplay_accepts_ = true; diagnostic_accepts_ = true;
        Core::Session gameplay(gameplay_), observer(diagnostic_);
        Scenario::Channels channels{{gameplay_, gameplay}, Scenario::Channel{diagnostic_, observer}};
        const auto effects_before = effects_;
        const auto entries_before = amount_entries_;
        const auto diagnostic_entries_before = diagnostic_entries_;
        const auto gameplay_steps_before = gameplay_progress_, diagnostic_steps_before = diagnostic_progress_;
        const auto command = diagnostic ? (pending ? "observe watch" : "observe echo 3") : (pending ? "slow 3" : "cast 3");
        const std::string value = diagnostic && pending ? "41" : "3";
        auto plan = Scenario::prepare(registry_, channels,
            std::string("script.version 1\nfixture.load counter\n") + command +
            " -> first\nretry $first -> duplicate\nawait $duplicate max_steps=2 -> ready\nretry $ready -> retried\n"
            "expect $first == " + value + "\nexpect $duplicate == " + value +
            "\nexpect $ready == " + value + "\nexpect $retried == " + value + "\nfinish");
        ASSERT_TRUE(plan);
        Scenario::Run run(*plan, registry_);
        ASSERT_EQ(run.step(), Scenario::Disposition::Running);
        ASSERT_EQ(run.results()[0].result.stage, pending ? Core::Stage::Pending : Core::Stage::Succeeded);
        gameplay_accepts_ = false; diagnostic_accepts_ = false;
        ASSERT_EQ(run.step(), Scenario::Disposition::Running);
        EXPECT_EQ(run.steps(), 1u);
        EXPECT_EQ(run.results()[0].request_id, run.results()[1].request_id);
        EXPECT_EQ(finish(run), Scenario::Disposition::Passed);
        EXPECT_EQ(run.error().failure, Core::Failure{});
        EXPECT_EQ(run.steps(), pending ? 3u : 1u);
        EXPECT_EQ(effects_, effects_before + (diagnostic ? 0 : 3));
        EXPECT_EQ(amount_entries_, entries_before + (diagnostic ? 0 : 1));
        EXPECT_EQ(diagnostic_entries_, diagnostic_entries_before + (diagnostic ? 1 : 0));
        EXPECT_EQ(gameplay_progress_, gameplay_steps_before + (diagnostic ? 0 : (pending ? 3 : 1)));
        EXPECT_EQ(diagnostic_progress_, diagnostic_steps_before + (diagnostic ? (pending ? 3 : 1) : 0));
        ASSERT_EQ(run.results().size(), 4u);
        for (const auto& result : run.results()) {
            EXPECT_TRUE(result.available); EXPECT_EQ(result.request_id, 1u);
            EXPECT_EQ(result.route, diagnostic ? Scenario::Route::Diagnostic : Scenario::Route::Gameplay);
            EXPECT_EQ(result.result.stage, Core::Stage::Succeeded);
            EXPECT_EQ(result.result.value, Core::Value::unsigned_integer(diagnostic && pending ? 41 : 3));
        }
        auto& selected = diagnostic ? observer : gameplay;
        EXPECT_EQ(selected.retained(), 1u); EXPECT_EQ((diagnostic ? gameplay : observer).retained(), 0u);
        auto retained = selected.lookup(1); ASSERT_TRUE(retained);
        EXPECT_EQ((**retained).stage, Core::Stage::Succeeded);
        EXPECT_EQ((**retained).value, run.results()[0].result.value);
        EXPECT_EQ(run.step(), Scenario::Disposition::Passed);
        EXPECT_EQ(effects_, effects_before + (diagnostic ? 0 : 3));
    }
}

TEST_F(ConsoleScenarioAuthority, ValidatesLaterLiteralBeforeEffects) {
    for (const auto* command : {"cast invalid", "observe echo invalid"}) {
        auto invalid = prepare(std::string("cast 3 -> first\n") + command + " -> second\nexpect $first == 3\nfinish");
        ASSERT_FALSE(invalid);
        EXPECT_EQ(invalid.error().failure, (Core::Failure{Core::Code::Type}));
        EXPECT_EQ(invalid.error().line, 4u);
        EXPECT_EQ(amount_entries_, 0u); EXPECT_EQ(diagnostic_entries_, 0u); EXPECT_EQ(effects_, 0u);
        EXPECT_EQ(gameplay_session_.retained(), 0u); EXPECT_EQ(diagnostic_session_.retained(), 0u);
    }
}

TEST_F(ConsoleScenarioAuthority, KeepsLegacyAndDirectAlreadyBoundAdmission) {
    auto typed = registry_.bind("cast", {Core::Value::unsigned_integer(3)}, gameplay_); ASSERT_TRUE(typed);
    auto text = Core::bind_text(registry_, gameplay_, "cast 3"); ASSERT_TRUE(text);
    auto legacy = Scenario::prepare(registry_, gameplay_,
        "script.version 1\nfixture.load counter\ncast 3 -> first\nretry $first -> duplicate\nexpect $duplicate == 3\nfinish");
    ASSERT_TRUE(legacy);
    Core::Session typed_session(gameplay_), text_session(gameplay_), legacy_session(gameplay_);
    Scenario::Run run(*legacy, registry_, legacy_session);
    gameplay_accepts_ = false; diagnostic_accepts_ = false;
    ASSERT_TRUE(typed_session.submit(1, *typed)); ASSERT_TRUE(typed_session.pump());
    ASSERT_TRUE(text_session.submit(1, *text)); ASSERT_TRUE(text_session.pump());
    EXPECT_EQ(finish(run), Scenario::Disposition::Passed);
    ASSERT_TRUE(typed_session.submit(1, *typed)); ASSERT_TRUE(text_session.submit(1, *text));
    EXPECT_EQ(effects_, 9u); EXPECT_EQ(amount_entries_, 3u); EXPECT_EQ(gameplay_progress_, 3u);
    EXPECT_EQ(diagnostic_entries_, 0u);
    EXPECT_EQ(legacy_session.retained(), 1u); EXPECT_EQ(typed_session.retained(), 1u); EXPECT_EQ(text_session.retained(), 1u);
}

TEST_F(ConsoleScenarioAuthority, RejectsLaterDiagnosticWithoutFallback) {
    ASSERT_TRUE(registry_.describe("cast", gameplay_)); ASSERT_TRUE(registry_.describe("read", diagnostic_));
    auto denied = prepare("cast 3 -> first\nread -> bad\nexpect $first == 3\nfinish");
    ASSERT_FALSE(denied); EXPECT_EQ(denied.error().failure.code, Core::Code::UnknownCommand);
    auto valid = prepare("cast 3 -> first\nobserve read -> seen\nexpect $seen == 3\nfinish"); ASSERT_TRUE(valid);
    for (const auto* command : {"observe setting 4", "observe cast 4", "observe missing", "observe test.run"}) {
        auto invalid = prepare(std::string("cast 3 -> first\n") + command + " -> bad\nexpect $first == 3\nfinish");
        ASSERT_FALSE(invalid); EXPECT_EQ(effects_, 0u);
    }
    Scenario::Channels disabled{{gameplay_, gameplay_session_}, std::nullopt};
    auto ordinary = Scenario::prepare(registry_, disabled,
        "script.version 1\nfixture.load counter\ncast 3 -> first\nexpect $first == 3\nfinish"); ASSERT_TRUE(ordinary);
    auto unavailable = Scenario::prepare(registry_, disabled,
        "script.version 1\nfixture.load counter\ncast 3 -> first\nobserve read -> bad\nexpect $first == 3\nfinish");
    ASSERT_FALSE(unavailable); EXPECT_EQ(unavailable.error().failure.code, Core::Code::Unauthorized);
    EXPECT_EQ(effects_, 0u); EXPECT_EQ(gameplay_session_.retained(), 0u); EXPECT_EQ(diagnostic_session_.retained(), 0u);
    Scenario::Run run(*ordinary, registry_);
    EXPECT_EQ(finish(run), Scenario::Disposition::Passed); EXPECT_EQ(effects_, 3u);
}

TEST_F(ConsoleScenarioAuthority, KeepsProducerContextForHandles) {
    auto gameplay_handle = gameplay_.resolve("actor", "actor"); ASSERT_TRUE(gameplay_handle);
    auto diagnostic_handle = diagnostic_.resolve("actor", "actor"); ASSERT_TRUE(diagnostic_handle);
    ASSERT_TRUE(Core::validate(Core::Value::handle(*gameplay_handle), handle_, gameplay_));
    ASSERT_TRUE(Core::validate(Core::Value::handle(*diagnostic_handle), handle_, diagnostic_));
    EXPECT_FALSE(Core::validate(Core::Value::handle(*diagnostic_handle), handle_, gameplay_));
    auto plan = prepare("observe handle -> observed\nhandle -> local_handle\n"
                        "expect $observed == actor\nexpect $local_handle == actor\nfinish"); ASSERT_TRUE(plan);
    for (const auto* producer : {"observe handle -> observed\nconsume $observed", "observe bundle -> observed\nconsume $observed.nested.actor"}) {
        auto invalid = prepare(std::string("cast 3 -> first\n") + producer + " -> bad\nexpect $first == 3\nfinish");
        ASSERT_FALSE(invalid); EXPECT_EQ(invalid.error().failure.code, Core::Code::Unauthorized);
        EXPECT_EQ(effects_, 0u);
    }
    Scenario::Run run(*plan, registry_); EXPECT_EQ(finish(run), Scenario::Disposition::Passed);
    ASSERT_EQ(run.results().size(), 2u);
    EXPECT_EQ(run.results()[0].result.value, Core::Value::handle(*diagnostic_handle));
    EXPECT_EQ(run.results()[1].result.value, Core::Value::handle(*gameplay_handle));
}

TEST_F(ConsoleScenarioAuthority, BindsSameRouteHandleAndDeveloperScalar) {
    auto plan = prepare("bundle -> bundle\nconsume $bundle.nested.actor -> key\n"
                        "observe read -> observed\ncast $observed -> casted\n"
                        "expect $key == 1\nexpect $casted == 0\nfinish"); ASSERT_TRUE(plan);
    Scenario::Run run(*plan, registry_); EXPECT_EQ(finish(run), Scenario::Disposition::Passed);
    EXPECT_EQ(effects_, 0u); EXPECT_EQ(gameplay_progress_, 1u);
}

TEST_F(ConsoleScenarioAuthority, RejectsCrossRouteHandlesEvenWithSharedIssuer) {
    auto actor = gameplay_.resolve("actor", "actor"); ASSERT_TRUE(actor);
    diagnostic_ = gameplay_authority_.issue(7);
    ASSERT_TRUE(gameplay_authority_.alias(diagnostic_, "actor", *actor));
    Core::Session diagnostic(diagnostic_);
    Scenario::Channels channels{{gameplay_, gameplay_session_}, Scenario::Channel{diagnostic_, diagnostic}};
    ASSERT_TRUE(registry_.describe("handle", diagnostic_));
    ASSERT_TRUE(registry_.bind("consume", {Core::Value::handle(*actor)}, gameplay_));
    auto local = Scenario::prepare(registry_, channels,
        "script.version 1\nfixture.load counter\nhandle -> local\nconsume $local -> key\nexpect $key == 1\nfinish");
    ASSERT_TRUE(local);
    auto crossing = Scenario::prepare(registry_, channels,
        "script.version 1\nfixture.load counter\ncast 3 -> first\nobserve handle -> seen\n"
        "consume $seen -> key\nexpect $key == 1\nfinish");
    ASSERT_FALSE(crossing); EXPECT_EQ(crossing.error().failure.code, Core::Code::Unauthorized);
    EXPECT_EQ(effects_, 0u);
}

TEST_F(ConsoleScenarioAuthority, BindsNewUnaliasedOperationsExactlyOnce) {
    std::size_t handlers = 0, minted = 0, consumed = 0;
    Core::Schema operation_schema; operation_schema.type = Core::Type::Handle; operation_schema.domain = "operation";
    Core::Schema inner; inner.type = Core::Type::Record; inner.fields = {"operation"}; inner.children = {operation_schema};
    Core::Schema output; output.type = Core::Type::Record; output.fields = {"nested"}; output.children = {inner};
    auto producer = registry_.add<>(descriptor("operation", 1, Core::Kind::Action, output),
        [&](const Core::Context&) {
            ++handlers; return std::make_shared<OperationWork>(gameplay_authority_, minted, effects_, true);
        });
    ASSERT_TRUE(producer);
    auto diagnostic_producer = registry_.add<>(descriptor("operation.view", 2, Core::Kind::Query, output),
        [&](const Core::Context&) {
            ++handlers; return std::make_shared<OperationWork>(diagnostic_authority_, minted, effects_, false);
        });
    ASSERT_TRUE(diagnostic_producer);
    auto consumer = registry_.add<Core::Handle>(descriptor("operation.read", 1, Core::Kind::Query, integer_, {{"operation", operation_schema}}),
        [&](const Core::Context&, Core::Handle operation) {
            ++consumed;
            return std::make_shared<SnapshotWork>(Core::Value::unsigned_integer(operation.key()));
        });
    ASSERT_TRUE(consumer);
    ASSERT_TRUE(gameplay_.aliases("operation").empty()); ASSERT_TRUE(diagnostic_.aliases("operation").empty());
    auto cross_gameplay = prepare("operation -> attempt\nawait $attempt max_steps=1 -> ready\n"
                                  "observe operation.read $ready.nested.operation -> read\nfinish");
    ASSERT_FALSE(cross_gameplay); EXPECT_EQ(cross_gameplay.error().failure.code, Core::Code::Unauthorized);
    EXPECT_EQ(cross_gameplay.error().line, 5u);
    auto cross_diagnostic = prepare("observe operation.view -> view\nawait $view max_steps=1 -> ready\n"
                                    "operation.read $ready.nested.operation -> read\nfinish");
    ASSERT_FALSE(cross_diagnostic); EXPECT_EQ(cross_diagnostic.error().failure.code, Core::Code::Unauthorized);
    EXPECT_EQ(cross_diagnostic.error().line, 5u);
    auto plan = prepare("operation -> attempt\nretry $attempt -> duplicate\nawait $duplicate max_steps=1 -> outcome\n"
                        "operation.read $outcome.nested.operation -> read\nretry $read -> reread\n"
                        "await $reread max_steps=0 -> checked\nexpect $checked == 2\n"
                        "observe operation.view -> view\nretry $view -> view_copy\nawait $view_copy max_steps=1 -> ready\n"
                        "observe operation.read $ready.nested.operation -> observed\nretry $observed -> observed_copy\n"
                        "expect $observed_copy == 2\nfinish");
    ASSERT_TRUE(plan); EXPECT_EQ(handlers, 0u); EXPECT_EQ(minted, 0u); EXPECT_EQ(consumed, 0u); EXPECT_EQ(effects_, 0u);
    Scenario::Run run(*plan, registry_); EXPECT_EQ(handlers, 0u); EXPECT_EQ(minted, 0u);
    EXPECT_EQ(finish(run), Scenario::Disposition::Passed);
    EXPECT_EQ(handlers, 2u); EXPECT_EQ(minted, 2u); EXPECT_EQ(consumed, 2u); EXPECT_EQ(effects_, 1u);
    EXPECT_TRUE(gameplay_.aliases("operation").empty()); EXPECT_TRUE(diagnostic_.aliases("operation").empty());
    ASSERT_EQ(run.results().size(), 11u);
    EXPECT_EQ(run.results()[0].request_id, run.results()[1].request_id);
    EXPECT_EQ(run.results()[3].request_id, run.results()[4].request_id);
    EXPECT_EQ(run.results()[6].route, Scenario::Route::Diagnostic);
    EXPECT_EQ(run.results()[6].request_id, run.results()[7].request_id);
    EXPECT_EQ(gameplay_session_.retained(), 2u); EXPECT_EQ(diagnostic_session_.retained(), 2u);
}

TEST_F(ConsoleScenarioAuthority, RejectsClosedEmptyChannelsAtPrepareAndConstruction) {
    const std::string script = "script.version 1\nfixture.load counter\ncast 3 -> first\nexpect $first == 3\nfinish";
    for (bool diagnostic_closed : {false, true}) {
        Core::Session gameplay(gameplay_), diagnostic(diagnostic_);
        Scenario::Channels channels{{gameplay_, gameplay}, Scenario::Channel{diagnostic_, diagnostic}};
        ASSERT_TRUE(gameplay.accepting()); ASSERT_TRUE(diagnostic.accepting());
        auto plan = Scenario::prepare(registry_, channels, script); ASSERT_TRUE(plan);
        if (diagnostic_closed) diagnostic.close(); else gameplay.close();
        EXPECT_EQ(gameplay.retained(), 0u); EXPECT_EQ(diagnostic.retained(), 0u);
        auto closed = Scenario::prepare(registry_, channels, script); ASSERT_FALSE(closed);
        EXPECT_EQ(closed.error().failure.code, Core::Code::Unauthorized);
        EXPECT_EQ(closed.error().line, 0u);
        EXPECT_THROW(static_cast<void>(Scenario::Run(*plan, registry_)), std::invalid_argument);
        EXPECT_EQ(effects_, 0u); EXPECT_EQ(gameplay_progress_, 0u); EXPECT_EQ(diagnostic_progress_, 0u);
    }
}

TEST_F(ConsoleScenarioAuthority, RoutesPendingRetryAndAwaitBySession) {
    auto plan = prepare("slow 3 -> casted\nobserve watch -> watched\nretry $watched -> duplicate\n"
                        "await $casted max_steps=2 -> cast_done\nawait $duplicate max_steps=2 -> watch_done\n"
                        "expect $cast_done == 3\nexpect $watch_done == 41\nfinish"); ASSERT_TRUE(plan);
    Scenario::Run run(*plan, registry_);
    ASSERT_EQ(run.step(), Scenario::Disposition::Running); EXPECT_EQ(gameplay_progress_, 1u);
    ASSERT_EQ(run.step(), Scenario::Disposition::Running); EXPECT_EQ(diagnostic_progress_, 1u);
    ASSERT_EQ(run.step(), Scenario::Disposition::Running); EXPECT_EQ(diagnostic_progress_, 1u);
    ASSERT_EQ(run.step(), Scenario::Disposition::Running); EXPECT_EQ(gameplay_progress_, 2u);
    EXPECT_EQ(diagnostic_progress_, 1u);
    EXPECT_EQ(finish(run), Scenario::Disposition::Passed);
    EXPECT_EQ(run.steps(), 6u); EXPECT_EQ(effects_, 3u);
    ASSERT_EQ(run.results().size(), 5u);
    EXPECT_EQ(run.results()[0].request_id, run.results()[1].request_id);
    EXPECT_EQ(run.results()[2].route, Scenario::Route::Diagnostic);
    EXPECT_EQ(run.results()[4].result.value, Core::Value::unsigned_integer(41));
}

TEST_F(ConsoleScenarioAuthority, SupportsExplicitExpectedOutcomes) {
    ASSERT_TRUE(registry_.describe("reject", gameplay_)); ASSERT_TRUE(registry_.describe("broken", diagnostic_));
    auto plan = prepare("refuse reject -> refused\nobserve refuse reject -> observed_refusal\n"
                        "error Owner broken -> failed\nobserve error Owner broken -> observed_error\n"
                        "observe read -> seen\nexpect $seen == 0\nfinish"); ASSERT_TRUE(plan);
    auto invalid = prepare("observe error Allocation broken -> failed\nexpect $failed == 0\nfinish");
    ASSERT_FALSE(invalid); EXPECT_EQ(invalid.error().failure.code, Core::Code::Unsupported);
    Scenario::Run run(*plan, registry_); EXPECT_EQ(finish(run), Scenario::Disposition::Passed);
    ASSERT_EQ(run.results().size(), 5u);
    EXPECT_EQ(run.results()[0].result.stage, Core::Stage::Refused);
    EXPECT_EQ(run.results()[3].result.failure.code, Core::Code::Owner);
    EXPECT_EQ(effects_, 0u);
}

TEST_F(ConsoleScenarioAuthority, RejectsMismatchedAndUsedChannels) {
    auto plan = prepare("local -> first\nexpect $first == 0\nfinish"); ASSERT_TRUE(plan);
    Scenario::Channels mismatch{{gameplay_, diagnostic_session_}, std::nullopt};
    EXPECT_FALSE(Scenario::prepare(registry_, mismatch, "script.version 1\nfixture.load counter\nlocal -> first\nexpect $first == 0\nfinish"));
    Scenario::Channels duplicate{{gameplay_, gameplay_session_}, Scenario::Channel{gameplay_, gameplay_session_}};
    EXPECT_FALSE(Scenario::prepare(registry_, duplicate, "script.version 1\nfixture.load counter\nlocal -> first\nexpect $first == 0\nfinish"));
    Core::Session same_context(gameplay_);
    Scenario::Channels same_scope{{gameplay_, gameplay_session_}, Scenario::Channel{gameplay_, same_context}};
    EXPECT_FALSE(Scenario::prepare(registry_, same_scope, "script.version 1\nfixture.load counter\nlocal -> first\nexpect $first == 0\nfinish"));
    auto request = registry_.bind("local", {}, gameplay_); ASSERT_TRUE(request);
    ASSERT_TRUE(gameplay_session_.submit(1, *request));
    EXPECT_FALSE(prepare("local -> first\nexpect $first == 0\nfinish"));
    EXPECT_THROW(static_cast<void>(Scenario::Run(*plan, registry_)), std::invalid_argument);
    EXPECT_EQ(effects_, 0u);
}

TEST_F(ConsoleScenarioAuthority, RejectsWrongRegistryAndLegacyDowngrade) {
    auto plan = prepare("local -> first\nexpect $first == 0\nfinish"); ASSERT_TRUE(plan);
    Core::Registry foreign;
    EXPECT_THROW(static_cast<void>(Scenario::Run(*plan, foreign)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(Scenario::Run(*plan, registry_, gameplay_session_)), std::invalid_argument);
    auto legacy = Scenario::prepare(registry_, gameplay_,
        "script.version 1\nfixture.load counter\nlocal -> first\nexpect $first == 0\nfinish"); ASSERT_TRUE(legacy);
    EXPECT_THROW(static_cast<void>(Scenario::Run(*legacy, registry_)), std::invalid_argument);
    Scenario::Run run(*plan, registry_);
    EXPECT_THROW(static_cast<void>(Scenario::Run(*plan, registry_)), std::invalid_argument);
    EXPECT_EQ(finish(run), Scenario::Disposition::Passed);
}

TEST_F(ConsoleScenarioAuthority, PinsDynamicRegistrationGeneration) {
    auto plan = prepare("cast 3 -> first\ncast $first -> second\nexpect $second == 3\nfinish"); ASSERT_TRUE(plan);
    Scenario::Run run(*plan, registry_);
    ASSERT_EQ(run.step(), Scenario::Disposition::Running); ASSERT_EQ(effects_, 3u);
    registrations_[0].reset(); ASSERT_TRUE(amount("cast", Core::Kind::Action, 1, 1));
    ASSERT_TRUE(registry_.describe("cast", gameplay_));
    EXPECT_EQ(run.step(), Scenario::Disposition::Failed);
    EXPECT_EQ(run.error().failure.code, Core::Code::Unregistered);
    EXPECT_EQ(effects_, 3u); EXPECT_EQ(gameplay_progress_, 1u);
    EXPECT_EQ(run.results()[0].result.stage, Core::Stage::Succeeded);
}

TEST_F(ConsoleScenarioAuthority, RejectsStaleStaticRegistration) {
    auto plan = prepare("cast 3 -> first\nexpect $first == 3\nfinish"); ASSERT_TRUE(plan);
    registrations_[0].reset(); ASSERT_TRUE(amount("cast", Core::Kind::Action, 1, 1));
    ASSERT_TRUE(registry_.describe("cast", gameplay_));
    Scenario::Run run(*plan, registry_);
    EXPECT_EQ(run.step(), Scenario::Disposition::Incomplete);
    EXPECT_EQ(run.error().failure.code, Core::Code::Unregistered); EXPECT_EQ(effects_, 0u);
}

TEST_F(ConsoleScenarioAuthority, ExpiresWithoutReplayAndRetainsEffects) {
    auto plan = prepare("cast 3 -> first\nretry $first -> duplicate\nexpect $duplicate == 3\nfinish"); ASSERT_TRUE(plan);
    Scenario::Run run(*plan, registry_);
    ASSERT_EQ(run.step(), Scenario::Disposition::Running); ASSERT_EQ(effects_, 3u);
    diagnostic_authority_.expire();
    EXPECT_EQ(run.step(), Scenario::Disposition::Incomplete);
    EXPECT_EQ(run.error().failure.code, Core::Code::Expired);
    EXPECT_EQ(run.results()[0].result.stage, Core::Stage::Succeeded); EXPECT_EQ(effects_, 3u);
    EXPECT_EQ(run.step(), Scenario::Disposition::Incomplete); EXPECT_EQ(effects_, 3u);
}

TEST_F(ConsoleScenarioAuthority, RejectsExpiryBeforeConstruction) {
    auto plan = prepare("local -> first\nexpect $first == 0\nfinish"); ASSERT_TRUE(plan);
    gameplay_authority_.expire();
    EXPECT_THROW(static_cast<void>(Scenario::Run(*plan, registry_)), std::invalid_argument);
    EXPECT_FALSE(prepare("local -> first\nexpect $first == 0\nfinish"));
    EXPECT_EQ(effects_, 0u);
}

TEST_F(ConsoleScenarioAuthority, ClosesBothPendingSessions) {
    auto plan = prepare("slow 3 -> casted\nobserve watch -> watched\nexpect $casted == 3\nfinish"); ASSERT_TRUE(plan);
    Scenario::Run run(*plan, registry_);
    ASSERT_EQ(run.step(), Scenario::Disposition::Running);
    ASSERT_EQ(run.step(), Scenario::Disposition::Running);
    ASSERT_EQ(run.results()[0].result.stage, Core::Stage::Pending);
    ASSERT_EQ(run.results()[1].result.stage, Core::Stage::Pending);
    run.close(); EXPECT_EQ(run.disposition(), Scenario::Disposition::Cancelled);
    EXPECT_EQ(run.results()[0].result.stage, Core::Stage::Cancelled);
    EXPECT_EQ(run.results()[1].result.stage, Core::Stage::Cancelled);
    auto gameplay = gameplay_session_.lookup(1); ASSERT_TRUE(gameplay);
    auto diagnostic = diagnostic_session_.lookup(1); ASSERT_TRUE(diagnostic);
    EXPECT_EQ((**gameplay).failure.code, Core::Code::Cancelled);
    EXPECT_EQ((**diagnostic).failure.code, Core::Code::Cancelled);
    EXPECT_EQ(effects_, 0u); EXPECT_EQ(run.steps(), 2u);
    EXPECT_EQ(run.step(), Scenario::Disposition::Cancelled);
}

TEST_F(ConsoleScenarioAuthority, PreservesOriginalFailureAfterClose) {
    auto plan = prepare("cast 3 -> first\nexpect $first == 4\nfinish"); ASSERT_TRUE(plan);
    Scenario::Run run(*plan, registry_);
    EXPECT_EQ(finish(run), Scenario::Disposition::Failed);
    const auto original = run.error(); ASSERT_EQ(original.failure.code, Core::Code::Type);
    run.close(Scenario::Disposition::Incomplete);
    EXPECT_EQ(run.error().failure, original.failure); EXPECT_EQ(run.error().line, original.line);
    EXPECT_EQ(run.results()[0].result.stage, Core::Stage::Succeeded); EXPECT_EQ(effects_, 3u);
}

TEST_F(ConsoleScenarioAuthority, PreservesLateOwnerFailureAtFinish) {
    auto installed = registry_.add<>(descriptor("late", 1, Core::Kind::Query, integer_),
        [](const Core::Context&) { return std::make_shared<DeferredFailureWork>(); }); ASSERT_TRUE(installed);
    auto plan = prepare("late -> failed\nlocal -> later\nexpect $later == 0\nfinish"); ASSERT_TRUE(plan);
    Scenario::Run run(*plan, registry_);
    ASSERT_EQ(run.step(), Scenario::Disposition::Running);
    ASSERT_EQ(run.results()[0].result.stage, Core::Stage::Pending);
    EXPECT_EQ(finish(run), Scenario::Disposition::Failed);
    const Core::Failure failure{Core::Code::Range, 7, 2};
    EXPECT_EQ(run.error().failure, failure);
    run.close(Scenario::Disposition::Incomplete); EXPECT_EQ(run.error().failure, failure);
    EXPECT_EQ(effects_, 0u);
}

TEST_F(ConsoleScenarioAuthority, BoundsAwaitAndClosesAllWork) {
    auto plan = prepare("slow 3 -> casted\nobserve watch -> watched\nawait $casted max_steps=0 -> outcome\n"
                        "expect $outcome == 3\nfinish"); ASSERT_TRUE(plan);
    Scenario::Run run(*plan, registry_);
    EXPECT_EQ(finish(run), Scenario::Disposition::Incomplete);
    EXPECT_EQ(run.error().failure.code, Core::Code::Limit); EXPECT_EQ(run.steps(), 2u);
    EXPECT_EQ(gameplay_progress_, 1u); EXPECT_EQ(diagnostic_progress_, 1u);
    EXPECT_EQ(run.results()[0].result.stage, Core::Stage::Cancelled);
    EXPECT_EQ(run.results()[1].result.stage, Core::Stage::Cancelled);
    EXPECT_EQ(effects_, 0u);
}

TEST_F(ConsoleScenarioAuthority, PreservesLegacySingleContextGrammar) {
    ASSERT_TRUE(snapshot("observe", 2, integer_, Core::Stage::Succeeded));
    auto legacy = Scenario::prepare(registry_, diagnostic_,
        "script.version 1\nfixture.load counter\nobserve -> seen\nread -> ordinary\n"
        "expect $seen == 0\nexpect $ordinary == 0\nfinish"); ASSERT_TRUE(legacy);
    Core::Session session(diagnostic_);
    Scenario::Run run(*legacy, registry_, session);
    EXPECT_EQ(finish(run), Scenario::Disposition::Passed);
    EXPECT_EQ(session.retained(), 2u); EXPECT_EQ(effects_, 0u);
}
