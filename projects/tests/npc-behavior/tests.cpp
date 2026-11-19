#include "scenario.h"

#include <array>
#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <typeinfo>

namespace {
    using namespace NpcBehavior;

    void require(bool condition, const char* message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    template <class Error, class Function> void expect_throw(Function&& function) {
        try {
            function();
        } catch (const Error& error) {
            require(typeid(error) == typeid(Error), "operation must throw the exact expected exception type");
            return;
        }
        throw std::runtime_error("operation did not reject invalid input");
    }

    bool same_position(glm::vec2 first, glm::vec2 second) {
        return first.x == second.x && first.y == second.y;
    }

    bool same_target(const std::optional<glm::vec2>& first, const std::optional<glm::vec2>& second) {
        return first.has_value() == second.has_value() && (!first || same_position(*first, *second));
    }

    bool same_trace(const Trace& first, const Trace& second) {
        return same_position(first.actor_position, second.actor_position) && first.actor_speed == second.actor_speed &&
               first.seconds == second.seconds && same_target(first.target, second.target) && first.left == second.left &&
               first.right == second.right && first.notice_distance == second.notice_distance &&
               first.lane_half_width == second.lane_half_width && first.sensor_evaluated == second.sensor_evaluated &&
               first.absolute_dx == second.absolute_dx && first.absolute_dy == second.absolute_dy && first.in_lane == second.in_lane &&
               first.in_range == second.in_range && first.detected == second.detected && first.patrol_called == second.patrol_called &&
               first.action == second.action && first.reason == second.reason && first.horizontal_command == second.horizontal_command;
    }

    bool same_record(const StepRecord& first, const StepRecord& second) {
        return same_position(first.before, second.before) && same_position(first.after, second.after) &&
               first.decision.command.horizontal == second.decision.command.horizontal &&
               same_trace(first.decision.trace, second.decision.trace);
    }

    void expect_step(const StepRecord& record, Action action, Reason reason, double command, float before_x, float after_x) {
        const auto& trace = record.decision.trace;
        require(trace.action == action && trace.reason == reason, "step must select the literal action and reason");
        require(std::abs(record.decision.command.horizontal - command) <= 1e-12, "step must return the literal command");
        require(trace.horizontal_command == record.decision.command.horizontal, "trace must explain the exact returned command");
        require(record.before.x == before_x && record.after.x == after_x, "real Character must have the literal before/after X");
        require(record.before.y == 220 && record.after.y == 220, "real Character must remain in lane220");
        require(same_position(trace.actor_position, record.before) && trace.actor_speed == 80, "trace must snapshot actual actor facts");
        require(
            trace.left == 80 && trace.right == 500 && trace.notice_distance == 120 && trace.lane_half_width == 16,
            "trace must retain the selected host policy"
        );
        require(trace.patrol_called == (action == Action::Patrol), "only Patrol may call the stateful navigation owner");
    }

    void notice_stop() {
        Scenario scenario;
        const auto record = scenario.step({glm::vec2{160, 220}, 0.5});
        expect_step(record, Action::Watch, Reason::TargetDetected, 0, 80, 80);
        const auto& trace = record.decision.trace;
        require(trace.sensor_evaluated && trace.in_lane && trace.in_range && trace.detected, "near target must be detected");
        require(trace.target && same_position(*trace.target, {160, 220}) && trace.seconds == 0.5, "trace must own its input facts");
        require(trace.absolute_dx == 80 && trace.absolute_dy == 0, "Watch must retain measured deltas");
        require(scenario.actor().position().x == 80 && scenario.records().size() == 1, "host must update and record exactly once");
        auto baseline = make_actor();
        Tutorial::PatrolController patrol(80, 500);
        baseline.update(patrol.read(baseline, 0.5), 0.5);
        require(baseline.position().x == 120, "unwrapped actual patrol must move the control actor");
    }

    void notice_boundary() {
        Scenario scenario;
        const auto boundary = scenario.step({glm::vec2{200, 236}, 0.5});
        expect_step(boundary, Action::Watch, Reason::TargetDetected, 0, 80, 80);
        require(
            boundary.decision.trace.absolute_dx == 120 && boundary.decision.trace.absolute_dy == 16 && boundary.decision.trace.in_lane &&
                boundary.decision.trace.in_range && boundary.decision.trace.detected,
            "inclusive rectangular corner must detect, without a Euclidean radius test"
        );
        const auto opposite = scenario.step({glm::vec2{-40, 204}, 0.5});
        expect_step(opposite, Action::Watch, Reason::TargetDetected, 0, 80, 80);
        require(
            opposite.decision.trace.absolute_dx == 120 && opposite.decision.trace.absolute_dy == 16,
            "proximity must use absolute deltas in both directions"
        );
    }

    void range_miss() {
        Scenario scenario;
        const auto record = scenario.step({glm::vec2{201, 220}, 0.5});
        expect_step(record, Action::Patrol, Reason::OutOfRange, 1, 80, 120);
        const auto& trace = record.decision.trace;
        require(trace.sensor_evaluated && trace.in_lane && !trace.in_range && !trace.detected, "range miss must retain sensor facts");
        require(trace.absolute_dx == 121 && trace.absolute_dy == 0, "range miss must retain actual distances");
    }

    void lane_miss() {
        Scenario scenario;
        const auto record = scenario.step({glm::vec2{160, 237}, 0.5});
        expect_step(record, Action::Patrol, Reason::OutOfLane, 1, 80, 120);
        const auto& trace = record.decision.trace;
        require(trace.sensor_evaluated && !trace.in_lane && trace.in_range && !trace.detected, "lane miss must retain sensor facts");
        require(trace.absolute_dx == 80 && trace.absolute_dy == 17, "lane miss must retain actual distances");
        Scenario both_miss;
        const auto both = both_miss.step({glm::vec2{201, 237}, 0.5});
        expect_step(both, Action::Patrol, Reason::OutOfLane, 1, 80, 120);
        require(!both.decision.trace.in_lane && !both.decision.trace.in_range, "OutOfLane must win when both predicates fail");
    }

    void no_target() {
        Scenario scenario;
        const auto record = scenario.step({std::nullopt, 0.5});
        expect_step(record, Action::Patrol, Reason::NoTarget, 1, 80, 120);
        const auto& trace = record.decision.trace;
        require(
            !trace.target && !trace.sensor_evaluated && !trace.absolute_dx && !trace.absolute_dy,
            "missing target must not invent a sensor measurement"
        );
        require(!trace.in_lane && !trace.in_range && !trace.detected, "unevaluated sensor flags must not claim detection");
    }

    void resume_patrol() {
        Scenario scenario;
        const auto watch = scenario.step({glm::vec2{160, 220}, 0.5});
        const auto resume = scenario.step({std::nullopt, 0.5});
        expect_step(watch, Action::Watch, Reason::TargetDetected, 0, 80, 80);
        expect_step(resume, Action::Patrol, Reason::NoTarget, 1, 80, 120);
        require(scenario.records().size() == 2, "host must publish exactly one record per update");
        Scenario leaves_range;
        static_cast<void>(leaves_range.step({glm::vec2{160, 220}, 0.5}));
        expect_step(leaves_range.step({glm::vec2{201, 220}, 0.5}), Action::Patrol, Reason::OutOfRange, 1, 80, 120);
        Scenario leaves_lane;
        static_cast<void>(leaves_lane.step({glm::vec2{160, 220}, 0.5}));
        expect_step(leaves_lane.step({glm::vec2{160, 237}, 0.5}), Action::Patrol, Reason::OutOfLane, 1, 80, 120);
    }

    void long_step() {
        Scenario scenario;
        const auto right = scenario.step({std::nullopt, 10});
        const auto left = scenario.step({std::nullopt, 10});
        expect_step(right, Action::Patrol, Reason::NoTarget, 0.525, 80, 500);
        expect_step(left, Action::Patrol, Reason::NoTarget, -0.525, 500, 80);
        require(right.decision.trace.seconds == 10 && left.decision.trace.seconds == 10, "trace must retain long elapsed time");
    }

    void rounded_application() {
        constexpr float large = 0x1p61f;
        Scenario scenario({large, 220}, large, 80, large);
        expect_throw<std::invalid_argument>([&] { static_cast<void>(scenario.step({std::nullopt, 1})); });
        require(
            same_position(scenario.actor().position(), {large, 220}) && scenario.records().empty(),
            "rounded endpoint application must reject before moving or publishing X0 outside [80,2^61]"
        );
        Scenario twin({large, 220}, large, 80, large);
        const auto continued = scenario.step({std::nullopt, 0.5});
        const auto expected = twin.step({std::nullopt, 0.5});
        require(
            same_record(continued, expected) && continued.decision.command.horizontal == -1 && continued.after.x == 0x1p60f &&
                scenario.records().size() == 1,
            "a safe large-domain continuation must agree with the untouched host"
        );

        Scenario prefix({large, 220}, large, 80, large);
        const auto prior = prefix.step({glm::vec2{large, 220}, 1});
        const std::array interrupted{StepInput{std::nullopt, 1}, StepInput{std::nullopt, 0.5}};
        expect_throw<std::invalid_argument>([&] { static_cast<void>(prefix.run(interrupted)); });
        require(
            prefix.records().size() == 1 && same_record(prefix.records()[0], prior) &&
                same_position(prefix.actor().position(), {large, 220}),
            "unsafe application must preserve the existing owned trace and not run the remaining batch"
        );
        require(same_record(prefix.step({std::nullopt, 0.5}), expected), "batch refusal must leave a valid continuation available");

        // A later endpoint read would reverse again, masking a rejected read's latent reversal.
        SentryController tested(80, large);
        SentryController control(80, large);
        auto endpoint = make_actor({large, 220}, large);
        expect_throw<std::invalid_argument>([&] { static_cast<void>(tested.read(endpoint, std::nullopt, 1)); });
        auto interior = make_actor({0x1p60f, 220}, large);
        auto control_interior = make_actor({0x1p60f, 220}, large);
        const auto next = tested.read(interior, std::nullopt, 0.25);
        const auto control_next = control.read(control_interior, std::nullopt, 0.25);
        require(
            next.command.horizontal == 1 && same_trace(next.trace, control_next.trace) && endpoint.position().x == large,
            "rejected rounded application must not reverse live patrol direction"
        );
        interior.update(next.command, 0.25);
        control_interior.update(control_next.command, 0.25);
        require(
            interior.position().x == 0x1.8p60f && same_position(interior.position(), control_interior.position()),
            "the preserved rightward command must have the same actual Character consequence"
        );
    }

    void watch_direction() {
        Scenario scenario;
        expect_step(scenario.step({std::nullopt, 10}), Action::Patrol, Reason::NoTarget, 0.525, 80, 500);
        expect_step(scenario.step({std::nullopt, 0.5}), Action::Patrol, Reason::NoTarget, -1, 500, 460);
        expect_step(scenario.step({glm::vec2{460, 220}, 0.5}), Action::Watch, Reason::TargetDetected, 0, 460, 460);
        expect_step(scenario.step({std::nullopt, 0.5}), Action::Patrol, Reason::NoTarget, -1, 460, 420);

        // A Watch at an endpoint must not perform even a latent patrol-direction change.
        SentryController controller;
        auto endpoint = make_actor({500, 220});
        const auto watch = controller.read(endpoint, endpoint.position(), 0.5);
        require(watch.trace.action == Action::Watch && !watch.trace.patrol_called, "endpoint Watch must bypass patrol");
        endpoint.update(watch.command, 0.5);
        auto interior = make_actor({200, 220});
        const auto resumed = controller.read(interior, std::nullopt, 0.5);
        require(resumed.command.horizontal == 1, "Watch must not reverse a pending rightward patrol at the endpoint");
        interior.update(resumed.command, 0.5);
        require(interior.position().x == 240 && endpoint.position().x == 500, "host must apply only the selected commands");
    }

    void zero_time() {
        Scenario scenario;
        const auto hold = scenario.step({glm::vec2{160, 220}, 0});
        expect_step(hold, Action::Hold, Reason::ZeroTime, 0, 80, 80);
        const auto& trace = hold.decision.trace;
        require(trace.target && same_position(*trace.target, {160, 220}) && trace.seconds == 0, "Hold must retain validated target/time");
        require(
            !trace.sensor_evaluated && !trace.absolute_dx && !trace.absolute_dy && !trace.in_lane && !trace.in_range && !trace.detected,
            "zero time must not evaluate perception"
        );
        expect_step(scenario.step({std::nullopt, 0.5}), Action::Patrol, Reason::NoTarget, 1, 80, 120);
        SentryController controller;
        auto endpoint = make_actor({500, 220});
        const auto endpoint_hold = controller.read(endpoint, std::nullopt, 0);
        require(endpoint_hold.trace.action == Action::Hold && !endpoint_hold.trace.patrol_called, "Hold must bypass endpoint reversal");
        endpoint.update(endpoint_hold.command, 0);
        auto interior = make_actor({200, 220});
        require(controller.read(interior, std::nullopt, 0.5).command.horizontal == 1, "Hold must preserve latent patrol direction");
    }

    void invalid_input() {
        const auto nan = std::numeric_limits<float>::quiet_NaN();
        const auto inf = std::numeric_limits<float>::infinity();
        const auto double_nan = std::numeric_limits<double>::quiet_NaN();
        const auto double_inf = std::numeric_limits<double>::infinity();
        Scenario positive;
        expect_step(positive.step({glm::vec2{160, 220}, 0.5}), Action::Watch, Reason::TargetDetected, 0, 80, 80);
        const std::array<std::array<float, 4>, 12> invalid_configurations{
            {{500, 80, 120, 16},
             {80, 80, 120, 16},
             {nan, 500, 120, 16},
             {-inf, 500, 120, 16},
             {80, nan, 120, 16},
             {80, inf, 120, 16},
             {80, 500, -1, 16},
             {80, 500, nan, 16},
             {80, 500, inf, 16},
             {80, 500, 120, -1},
             {80, 500, 120, nan},
             {80, 500, 120, inf}}
        };
        for (const auto& config : invalid_configurations) {
            expect_throw<std::invalid_argument>([&] { static_cast<void>(SentryController(config[0], config[1], config[2], config[3])); });
        }
        SentryController zero_policy(80, 500, 0, 0);
        auto exact = make_actor();
        require(zero_policy.read(exact, exact.position(), 0.5).trace.detected, "zero sensor extents are valid at exact coincidence");

        const std::array invalid_steps{
            StepInput{glm::vec2{nan, 220}, 0.5},
            StepInput{glm::vec2{inf, 220}, 0.5},
            StepInput{glm::vec2{160, nan}, 0.5},
            StepInput{glm::vec2{160, -inf}, 0.5},
            StepInput{glm::vec2{nan, 220}, 0},
            StepInput{glm::vec2{160, inf}, 0},
            StepInput{std::nullopt, -0.5},
            StepInput{std::nullopt, double_nan},
            StepInput{std::nullopt, double_inf},
            StepInput{std::nullopt, std::numeric_limits<double>::max()},
            StepInput{glm::vec2{160, 220}, std::numeric_limits<double>::max()}
        };
        for (const auto& input : invalid_steps) {
            Scenario scenario;
            const auto prior = scenario.step({std::nullopt, 0.5});
            expect_step(prior, Action::Patrol, Reason::NoTarget, 1, 80, 120);
            expect_throw<std::invalid_argument>([&] { static_cast<void>(scenario.step(input)); });
            require(
                scenario.actor().position().x == 120 && scenario.records().size() == 1 && same_record(scenario.records()[0], prior),
                "rejected step must not apply movement or publish a record"
            );
            expect_step(scenario.step({std::nullopt, 0.5}), Action::Patrol, Reason::NoTarget, 1, 120, 160);
        }

        Scenario partial_batch;
        const std::array interrupted{StepInput{std::nullopt, 0.5}, StepInput{glm::vec2{nan, 220}, 0.5}, StepInput{std::nullopt, 0.5}};
        expect_throw<std::invalid_argument>([&] { static_cast<void>(partial_batch.run(interrupted)); });
        require(
            partial_batch.records().size() == 1 && partial_batch.actor().position().x == 120,
            "non-capacity batch failure must preserve only its successful prefix"
        );
        expect_step(partial_batch.records()[0], Action::Patrol, Reason::NoTarget, 1, 80, 120);
        expect_step(partial_batch.step({std::nullopt, 0.5}), Action::Patrol, Reason::NoTarget, 1, 120, 160);

        for (const float outside_x : {79.0f, 501.0f}) {
            auto outside = make_actor({outside_x, 220});
            SentryController controller;
            expect_throw<std::invalid_argument>([&] { static_cast<void>(controller.read(outside, std::nullopt, 0.5)); });
            expect_throw<std::invalid_argument>([&] { static_cast<void>(controller.read(outside, std::nullopt, 0)); });
            require(outside.position().x == outside_x, "out-of-interval rejection must not move the actual actor");
            auto interior = make_actor({200, 220});
            require(controller.read(interior, std::nullopt, 0.5).command.horizontal == 1, "invalid actor must not alter patrol direction");
        }

        // Each late invalid input at the reversal endpoint must reject before mutating navigation.
        for (const auto& input : invalid_steps) {
            SentryController tested;
            SentryController twin;
            auto actor = make_actor();
            auto control = make_actor();
            actor.update(tested.read(actor, std::nullopt, 10).command, 10);
            control.update(twin.read(control, std::nullopt, 10).command, 10);
            require(actor.position().x == 500 && control.position().x == 500, "reversal fixture must first reach the endpoint");
            expect_throw<std::invalid_argument>([&] { static_cast<void>(tested.read(actor, input.target, input.seconds)); });
            require(actor.position().x == 500, "late invalid input must not move the actor");
            auto interior = make_actor({200, 220});
            auto control_interior = make_actor({200, 220});
            const auto next = tested.read(interior, std::nullopt, 0.5);
            const auto expected = twin.read(control_interior, std::nullopt, 0.5);
            require(
                next.command.horizontal == 1 && same_trace(next.trace, expected.trace), "invalid read must agree with untouched twin state"
            );
            interior.update(next.command, 0.5);
            control_interior.update(expected.command, 0.5);
            require(same_position(interior.position(), control_interior.position()), "twin actors must receive the same valid consequence");
        }
    }

    void owned_trace() {
        StepRecord retained;
        StepRecord original;
        {
            Scenario scenario;
            std::optional<glm::vec2> target = glm::vec2{160, 220};
            retained = scenario.step({target, 0.5});
            original = retained;
            target = glm::vec2{900, -500};
            static_cast<void>(scenario.step({target, 0.5}));
            require(same_record(retained, original), "later decisions and caller target replacement must not change retained data");
        }
        require(
            same_record(retained, original) && retained.decision.trace.target && same_position(*retained.decision.trace.target, {160, 220}),
            "trace must survive original actor/controller destruction"
        );
        expect_step(retained, Action::Watch, Reason::TargetDetected, 0, 80, 80);

        Scenario first;
        Scenario second;
        static_cast<void>(first.step({std::nullopt, 10}));
        expect_step(first.step({std::nullopt, 0.5}), Action::Patrol, Reason::NoTarget, -1, 500, 460);
        expect_step(second.step({std::nullopt, 0.5}), Action::Patrol, Reason::NoTarget, 1, 80, 120);
        expect_step(first.step({std::nullopt, 0.5}), Action::Patrol, Reason::NoTarget, -1, 460, 420);
    }

    void trace_capacity() {
        Scenario scenario;
        std::array<StepInput, Scenario::capacity> inputs{};
        const auto records = scenario.run(inputs);
        require(records.size() == 32 && scenario.records().size() == 32, "request32 must yield32 owned records");
        std::array<StepRecord, Scenario::capacity> retained{};
        for (std::size_t index = 0; index < retained.size(); ++index) {
            retained[index] = records[index];
            const auto& record = records[index];
            require(
                record.decision.trace.action == Action::Patrol && record.decision.trace.reason == Reason::NoTarget &&
                    record.decision.trace.patrol_called && record.before.y == 220 && record.after.y == 220 && record.after.x >= 80 &&
                    record.after.x <= 500,
                "bounded batch must execute the real patrol in its interval"
            );
            require(
                index == 0 ? record.before.x == 80 : same_position(record.before, records[index - 1].after),
                "batch records must describe consecutive actual updates"
            );
        }
        const auto before = scenario.actor().position();
        std::array<StepInput, Scenario::capacity + 1> oversize{};
        expect_throw<std::length_error>([&] { static_cast<void>(scenario.run(oversize)); });
        expect_throw<std::length_error>([&] { static_cast<void>(scenario.step({std::nullopt, 0.5})); });
        require(
            scenario.records().size() == 32 && same_position(before, scenario.actor().position()),
            "capacity refusal must happen before any movement or publication"
        );
        for (std::size_t index = 0; index < retained.size(); ++index)
            require(same_record(scenario.records()[index], retained[index]), "capacity refusal must preserve every prior owned record");
        require(scenario.run(std::span<const StepInput>{}).size() == 32, "empty batch at capacity must be a valid no-op");

        Scenario prefix;
        const auto prior = prefix.step({std::nullopt, 0.5});
        expect_throw<std::length_error>([&] { static_cast<void>(prefix.run(inputs)); });
        require(
            prefix.records().size() == 1 && same_record(prefix.records()[0], prior) && prefix.actor().position().x == 120,
            "batch exceeding remaining capacity must reject atomically, not consume its fitting prefix"
        );
        Scenario fresh;
        expect_throw<std::length_error>([&] { static_cast<void>(fresh.run(oversize)); });
        require(fresh.records().empty() && fresh.actor().position().x == 80, "fresh request33 must reject before execution");
    }

    struct Case {
        std::string_view name;
        void (*run)();
    };

    constexpr std::array cases{Case{"notice_stop", notice_stop}, Case{"notice_boundary", notice_boundary},
                               Case{"range_miss", range_miss},   Case{"lane_miss", lane_miss},
                               Case{"no_target", no_target},     Case{"resume_patrol", resume_patrol},
                               Case{"long_step", long_step},     Case{"rounded_application", rounded_application},
                               Case{"watch_direction", watch_direction},
                               Case{"zero_time", zero_time},     Case{"invalid_input", invalid_input},
                               Case{"owned_trace", owned_trace}, Case{"trace_capacity", trace_capacity}};
    static_assert(cases.size() == 13);
}

int main(int argc, char** argv) {
    try {
        if (argc != 2)
            throw std::invalid_argument("Specify exactly one case name");
        for (const auto& test : cases) {
            if (test.name == argv[1]) {
                test.run();
                std::cout << "npc_behavior." << test.name << ": assertions passed\n";
                return 0;
            }
        }
        throw std::invalid_argument("Unknown NPC case name");
    } catch (const std::exception& error) {
        std::cerr << "npc_behavior: " << error.what() << '\n';
        return 1;
    }
}
