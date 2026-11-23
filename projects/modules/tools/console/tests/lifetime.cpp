#include <cheryl/tools/console/parser.h>
#include <cheryl/tools/console/session.h>

#include <gtest/gtest.h>

namespace CE::Tools::Console {
    namespace {
        struct Target { unsigned effects = 0; };
        class Progress final : public Work {
            std::shared_ptr<Target> target_;
        public:
            explicit Progress(std::shared_ptr<Target> target) : target_(std::move(target)) {
                result.value = Value::unsigned_integer(0);
            }
            void step() noexcept override {
                if (result.stage == Stage::Queued) result.stage = Stage::Pending;
                else if (!terminal(result.stage)) {
                    ++target_->effects; result.value = Value::unsigned_integer(target_->effects); result.stage = Stage::Succeeded;
                }
            }
            void close() noexcept override { if (!terminal(result.stage)) step(); }
        };
        Descriptor descriptor() {
            Schema output; output.type = Type::Unsigned;
            return {"act", "Retains entered target.", "test", Kind::Action, Timing::OwnerProgress, 0, {}, output};
        }
    }
    TEST(ConsoleLifetime, OldGenerationNeverRedirectsQueuedWork) {
        Authority authority; auto context = authority.issue(0); Registry registry; Session session(context);
        auto first = std::make_shared<Target>(), second = std::make_shared<Target>();
        auto old = registry.add<>(descriptor(), [first](const Context&) { return std::make_shared<Progress>(first); });
        ASSERT_TRUE(old); auto request = registry.bind("act", {}, context); ASSERT_TRUE(request);
        ASSERT_TRUE(session.submit(1, *request)); old->reset();
        auto replacement = registry.add<>(descriptor(), [second](const Context&) { return std::make_shared<Progress>(second); });
        ASSERT_TRUE(replacement); auto next = registry.bind("act", {}, context); ASSERT_TRUE(next);
        EXPECT_FALSE(request->same_body(*next));
        auto conflict = session.submit(1, *next); ASSERT_FALSE(conflict); EXPECT_EQ(conflict.error().code, Code::Conflict);
        ASSERT_TRUE(session.pump()); EXPECT_EQ((*session.lookup(1))->stage, Stage::Cancelled);
        EXPECT_EQ(first->effects, 0u); EXPECT_EQ(second->effects, 0u);
    }
    TEST(ConsoleLifetime, EnteredTargetSurvivesUnregisterExpiryAndLostReply) {
        Authority authority; auto context = authority.issue(0); Registry registry; Session session(context, 1);
        auto target = std::make_shared<Target>(); std::weak_ptr<Target> lifetime = target;
        auto registration = registry.add<>(descriptor(), [target](const Context&) { return std::make_shared<Progress>(target); });
        ASSERT_TRUE(registration); auto request = registry.bind("act", {}, context); ASSERT_TRUE(request);
        ASSERT_TRUE(session.submit(1, *request)); ASSERT_TRUE(session.pump());
        EXPECT_EQ((*session.lookup(1))->stage, Stage::Pending);
        registration->reset(); target.reset(); authority.expire();
        ASSERT_TRUE(session.submit(1, *request)); EXPECT_FALSE(lifetime.expired());
        session.close(); ASSERT_TRUE(session.lookup(1)); EXPECT_EQ((*session.lookup(1))->stage, Stage::Succeeded);
        ASSERT_TRUE(session.submit(1, *request)); EXPECT_EQ(std::get<std::uint64_t>((*session.lookup(1))->value.data), 1u);
        EXPECT_FALSE(session.submit(2, *request));
    }
    TEST(ConsoleLifetime, CapacityNeverEvictsAndForeignSessionCannotAdmit) {
        Authority authority, foreign; auto context = authority.issue(0); Registry registry; Session session(context, 1);
        auto target = std::make_shared<Target>();
        auto registration = registry.add<>(descriptor(), [target](const Context&) { return std::make_shared<Progress>(target); });
        ASSERT_TRUE(registration); auto request = registry.bind("act", {}, context); ASSERT_TRUE(request);
        ASSERT_TRUE(session.submit(1, *request));
        auto full = session.submit(2, *request); ASSERT_FALSE(full); EXPECT_EQ(full.error().code, Code::Capacity);
        Session other(foreign.issue(0)); auto denied = other.submit(1, *request);
        ASSERT_FALSE(denied); EXPECT_EQ(denied.error().code, Code::Unauthorized);
        ASSERT_TRUE(session.pump()); ASSERT_TRUE(session.pump());
        ASSERT_TRUE(session.submit(1, *request)); EXPECT_EQ(target->effects, 1u);
        EXPECT_FALSE(session.submit(2, *request));
    }
    TEST(ConsoleLifetime, OwnerReentranceRefusesAndFinalRetentionReleasesAtSessionDestruction) {
        std::weak_ptr<Target> lifetime;
        {
            Authority authority; auto context = authority.issue(0); Registry registry; Session session(context);
            auto target = std::make_shared<Target>(); lifetime = target;
            Code nested = Code::None;
            auto registration = registry.add<>(descriptor(), [&session, &nested, target](const Context&) {
                const auto attempt = session.pump();
                if (!attempt) nested = attempt.error().code;
                return std::make_shared<Progress>(target);
            });
            ASSERT_TRUE(registration); auto request = registry.bind("act", {}, context); ASSERT_TRUE(request);
            ASSERT_TRUE(session.submit(1, *request)); ASSERT_TRUE(session.pump());
            EXPECT_EQ(nested, Code::Reentrant);
            registration->reset(); target.reset(); session.close();
            EXPECT_FALSE(lifetime.expired()); EXPECT_EQ((*session.lookup(1))->stage, Stage::Succeeded);
        }
        EXPECT_TRUE(lifetime.expired());
    }
    TEST(ConsoleLifetime, ResolvedGenerationSurvivesOnlyForInspectionAndExactRetry) {
        Authority authority; auto context = authority.issue(0); Registry registry; Session session(context);
        auto first = std::make_shared<Target>(), second = std::make_shared<Target>();
        auto old = registry.add<>(descriptor(), [first](const Context&) { return std::make_shared<Progress>(first); });
        ASSERT_TRUE(old);
        auto resolved = registry.resolve("act", context); ASSERT_TRUE(resolved);
        auto request = registry.bind(*resolved, {}); ASSERT_TRUE(request);
        ASSERT_TRUE(session.submit(1, *request)); ASSERT_TRUE(session.pump());
        old->reset();
        auto replacement = registry.add<>(descriptor(), [second](const Context&) { return std::make_shared<Progress>(second); });
        ASSERT_TRUE(replacement);
        auto stale = registry.bind(*resolved, {}); ASSERT_FALSE(stale); EXPECT_EQ(stale.error().code, Code::Unregistered);
        EXPECT_EQ(*resolved->descriptor(), descriptor());
        auto next = registry.resolve("act", context); ASSERT_TRUE(next);
        auto next_request = registry.bind(*next, {}); ASSERT_TRUE(next_request);
        auto conflict = session.submit(1, *next_request); ASSERT_FALSE(conflict); EXPECT_EQ(conflict.error().code, Code::Conflict);
        session.close(); EXPECT_FALSE(session.accepting());
        auto retry = session.submit(1, *request); ASSERT_TRUE(retry); EXPECT_EQ((*retry)->stage, Stage::Succeeded);
        EXPECT_EQ(first->effects, 1u); EXPECT_EQ(second->effects, 0u);
    }
    TEST(ConsoleLifetime, DestroyedRegistryCannotRebindRetainedResolution) {
        Authority authority; auto context = authority.issue(0);
        ResolvedCommand resolved;
        Registration registration;
        auto target = std::make_shared<Target>();
        {
            Registry registry;
            auto installed = registry.add<>(descriptor(), [target](const Context&) { return std::make_shared<Progress>(target); });
            ASSERT_TRUE(installed); registration = std::move(*installed);
            auto selected = registry.resolve("act", context); ASSERT_TRUE(selected); resolved = *selected;
            ASSERT_TRUE(registry.bind(resolved, {}));
        }
        Registry replacement;
        auto installed = replacement.add<>(descriptor(), [target](const Context&) { return std::make_shared<Progress>(target); });
        ASSERT_TRUE(installed);
        auto stale = replacement.bind(resolved, {}); ASSERT_FALSE(stale); EXPECT_EQ(stale.error().code, Code::Unregistered);
        ASSERT_NE(resolved.descriptor(), nullptr); EXPECT_EQ(*resolved.descriptor(), descriptor()); EXPECT_EQ(target->effects, 0u);
    }
    TEST(ConsoleLifetime, AdmissionReflectsLiveClosedAndPumpingState) {
        Authority authority; auto context = authority.issue(0); Registry registry; Session session(context);
        EXPECT_TRUE(session.accepting());
        bool during_handler = true;
        auto target = std::make_shared<Target>();
        auto registration = registry.add<>(descriptor(), [&](const Context&) {
            during_handler = session.accepting(); return std::make_shared<Progress>(target);
        });
        ASSERT_TRUE(registration); auto request = registry.bind("act", {}, context); ASSERT_TRUE(request);
        ASSERT_TRUE(session.submit(1, *request)); ASSERT_TRUE(session.pump()); EXPECT_FALSE(during_handler);
        EXPECT_TRUE(session.accepting()); session.close(); EXPECT_FALSE(session.accepting());
        ASSERT_TRUE(session.submit(1, *request)); EXPECT_EQ(target->effects, 1u);
        Session expired(context); EXPECT_TRUE(expired.accepting()); authority.expire(); EXPECT_FALSE(expired.accepting());
    }
}
