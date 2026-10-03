#include <gtest/gtest.h>

#include <templates/state-machine.h>
#include <templates/state-tracker.h>
#include <templates/versioned-variable.h>

#include <thread>
#include <vector>

TEST(state_tracker, successive_samples) {
    StateTracker<int> health{10};
    EXPECT_FALSE(health.changed());

    health.update(7);
    EXPECT_EQ(health.previous(), 10);
    EXPECT_EQ(health.current(), 7);
    EXPECT_EQ(health.delta(), -3);
    health.update(7);
    EXPECT_FALSE(health.changed());
}

TEST(versioned_variable, coherent_waiter_snapshot) {
    VersionedVariable<int> variable{0};
    const auto start = variable.snapshot();
    variable.set(0);
    EXPECT_EQ(variable.snapshot().revision, start.revision);

    // Retain the revision before starting the writer. The waiter cannot miss a change
    // even if the writer completes before wait_for_change() enters its wait.
    std::thread writer([&] { variable.set(10); });
    const auto changed = variable.wait_for_change(start.revision);
    writer.join();
    EXPECT_EQ(changed.value, 10);
    EXPECT_EQ(changed.revision, 1u);
    EXPECT_EQ(variable.snapshot().revision, changed.revision);
}

namespace {
    enum class Movement { Grounded, Airborne };

    enum class MoveTrigger { Jump, Land };

    enum class Posture { Standing, Crouched };

    enum class PostureTrigger { Crouch, Stand };
} // namespace

TEST(state_machine, guarded_transitions) {
    StateMachine<Posture, PostureTrigger> posture{Posture::Standing};
    StateMachine<Movement, MoveTrigger> movement{Movement::Grounded};
    std::vector<int> order;
    movement.on_exit(Movement::Grounded, [&] { order.push_back(1); });
    movement.on_enter(Movement::Airborne, [&] { order.push_back(3); });
    movement.add_transition(
        Movement::Grounded, MoveTrigger::Jump, Movement::Airborne, [&] { return posture.state() == Posture::Standing; },
        [&] { order.push_back(2); }
    );
    movement.add_transition(Movement::Airborne, MoveTrigger::Land, Movement::Grounded);
    posture.add_transition(Posture::Standing, PostureTrigger::Crouch, Posture::Crouched);
    posture.add_transition(Posture::Crouched, PostureTrigger::Stand, Posture::Standing);

    // One independent state domain can constrain another through an ordinary guard.
    EXPECT_TRUE(posture.trigger(PostureTrigger::Crouch));
    EXPECT_FALSE(movement.trigger(MoveTrigger::Jump));
    EXPECT_EQ(movement.state(), Movement::Grounded);
    EXPECT_FALSE(movement.trigger(MoveTrigger::Land));

    // Standing again enables Jump. Exit, transition action, and enter execute in order.
    EXPECT_TRUE(posture.trigger(PostureTrigger::Stand));
    EXPECT_TRUE(movement.trigger(MoveTrigger::Jump));
    EXPECT_EQ(movement.state(), Movement::Airborne);
    EXPECT_EQ(order, (std::vector<int>{1, 2, 3}));
    EXPECT_TRUE(movement.trigger(MoveTrigger::Land));
}
