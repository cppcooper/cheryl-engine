#include "scenario.h"

#include <array>
#include <cmath>
#include <exception>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
    using namespace NpcBehavior;

    std::string_view name(Action action) {
        switch (action) {
            case Action::Hold:
                return "Hold";
            case Action::Watch:
                return "Watch";
            case Action::Patrol:
                return "Patrol";
        }
        throw std::logic_error("Unknown sentry action");
    }

    std::string_view name(Reason reason) {
        switch (reason) {
            case Reason::ZeroTime:
                return "ZeroTime";
            case Reason::NoTarget:
                return "NoTarget";
            case Reason::OutOfLane:
                return "OutOfLane";
            case Reason::OutOfRange:
                return "OutOfRange";
            case Reason::TargetDetected:
                return "TargetDetected";
        }
        throw std::logic_error("Unknown sentry reason");
    }

    struct Expected {
        Action action;
        Reason reason;
        double command;
        float before_x;
        float after_x;
    };

    void check(const StepRecord& record, const StepInput& input, Expected expected) {
        const auto& trace = record.decision.trace;
        if (trace.action != expected.action || trace.reason != expected.reason ||
            std::abs(record.decision.command.horizontal - expected.command) > 1e-12 ||
            trace.horizontal_command != record.decision.command.horizontal || record.before.x != expected.before_x ||
            record.after.x != expected.after_x || record.before.y != 220 || record.after.y != 220 ||
            trace.actor_position.x != record.before.x || trace.actor_position.y != record.before.y || trace.actor_speed != 80 ||
            trace.seconds != input.seconds || trace.left != 80 || trace.right != 500 || trace.notice_distance != 120 ||
            trace.lane_half_width != 16 || trace.patrol_called != (expected.action == Action::Patrol) ||
            trace.target.has_value() != input.target.has_value())
            throw std::runtime_error("Real Character facts/action/command/consequence mismatch");
        if (input.target) {
            if (trace.target->x != input.target->x || trace.target->y != input.target->y || !trace.sensor_evaluated || !trace.in_lane ||
                !trace.in_range || !trace.detected ||
                trace.absolute_dx != std::abs(static_cast<double>(input.target->x) - record.before.x) ||
                trace.absolute_dy != std::abs(static_cast<double>(input.target->y) - record.before.y))
                throw std::runtime_error("Detected target snapshot or rectangular sensor facts mismatch");
        } else if (trace.sensor_evaluated || trace.absolute_dx || trace.absolute_dy || trace.in_lane || trace.in_range || trace.detected) {
            throw std::runtime_error("Missing target must not invent observations");
        }
    }

    void print(std::string_view sequence, std::size_t index, const StepRecord& record) {
        const auto& trace = record.decision.trace;
        std::cout << "sequence=" << sequence << " step=" << index << " actor=(" << trace.actor_position.x << ',' << trace.actor_position.y
                  << ") speed=" << trace.actor_speed << " seconds=" << trace.seconds << " target=";
        if (trace.target)
            std::cout << '(' << trace.target->x << ',' << trace.target->y << ')';
        else
            std::cout << "none";
        std::cout << " bounds=[" << trace.left << ',' << trace.right << "] notice=" << trace.notice_distance
                  << " lane_half_width=" << trace.lane_half_width << " sensor_evaluated=" << trace.sensor_evaluated << " abs_dx=";
        if (trace.absolute_dx)
            std::cout << *trace.absolute_dx;
        else
            std::cout << "none";
        std::cout << " abs_dy=";
        if (trace.absolute_dy)
            std::cout << *trace.absolute_dy;
        else
            std::cout << "none";
        std::cout << " in_lane=" << trace.in_lane << " in_range=" << trace.in_range << " detected=" << trace.detected
                  << " patrol_called=" << trace.patrol_called << " action=" << name(trace.action) << " reason=" << name(trace.reason)
                  << " command=" << record.decision.command.horizontal << " before=(" << record.before.x << ',' << record.before.y
                  << ") after=(" << record.after.x << ',' << record.after.y << ")\n";
    }

    template <std::size_t Count>
    void run(std::string_view sequence, const std::array<StepInput, Count>& inputs, const std::array<Expected, Count>& expected) {
        Scenario scenario;
        const auto records = scenario.run(inputs);
        if (records.size() != Count)
            throw std::runtime_error("Scenario must retain one record per actual Character update");
        for (std::size_t index = 0; index < Count; ++index) {
            check(records[index], inputs[index], expected[index]);
            print(sequence, index, records[index]);
        }
        if (scenario.actor().position().x != expected.back().after_x || scenario.actor().position().y != 220)
            throw std::runtime_error("Retained records must agree with the final actual Character position");
    }
}

int main() {
    try {
        std::cout << std::boolalpha << std::setprecision(12);
        const std::array notice{StepInput{glm::vec2{160, 220}, 0.5}, StepInput{std::nullopt, 0.5}};
        const std::array notice_expected{
            Expected{Action::Watch, Reason::TargetDetected, 0, 80, 80}, Expected{Action::Patrol, Reason::NoTarget, 1, 80, 120}
        };
        run("notice_resume", notice, notice_expected);
        const std::array leftward{
            StepInput{std::nullopt, 10}, StepInput{std::nullopt, 0.5}, StepInput{glm::vec2{460, 220}, 0.5}, StepInput{std::nullopt, 0.5}
        };
        const std::array leftward_expected{
            Expected{Action::Patrol, Reason::NoTarget, 0.525, 80, 500}, Expected{Action::Patrol, Reason::NoTarget, -1, 500, 460},
            Expected{Action::Watch, Reason::TargetDetected, 0, 460, 460}, Expected{Action::Patrol, Reason::NoTarget, -1, 460, 420}
        };
        run("leftward_watch", leftward, leftward_expected);
        std::cout << "limits=one optional target; unobstructed corridor; rectangular proximity, not line-of-sight; "
                     "unchanged patrol; no pursuit, collision, graphics or population acceptance\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "npc_behavior.consumer: " << error.what() << '\n';
        return 1;
    }
}
