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
}
