#include "scenario.h"

#include <array>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
    using namespace NpcBehavior;

    void require(bool condition, const char* message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    void notice_stop() {
        Scenario scenario;
        const auto record = scenario.step({glm::vec2{160, 220}, 0.5});
        const auto& trace = record.decision.trace;
        require(trace.action == Action::Watch && trace.reason == Reason::TargetDetected, "near target must select Watch");
        require(trace.sensor_evaluated && trace.detected && !trace.patrol_called, "Watch must not advance patrol");
        require(trace.absolute_dx == 80 && trace.absolute_dy == 0, "Watch must retain measured deltas");
        require(record.decision.command.horizontal == 0 && trace.horizontal_command == 0, "Watch command must be zero");
        require(record.before.x == 80 && record.after.x == 80 && record.after.y == 220, "real Character must stop in lane220");
        auto baseline = make_actor();
        Tutorial::PatrolController patrol(80, 500);
        baseline.update(patrol.read(baseline, 0.5), 0.5);
        require(baseline.position().x == 120, "unwrapped actual patrol must move the control actor");
    }

    void resume_patrol() {
        Scenario scenario;
        const auto watch = scenario.step({glm::vec2{160, 220}, 0.5});
        const auto resume = scenario.step({std::nullopt, 0.5});
        require(watch.after.x == 80 && resume.before.x == 80 && resume.after.x == 120, "lost target must resume immediately");
        require(
            resume.decision.trace.action == Action::Patrol && resume.decision.trace.reason == Reason::NoTarget,
            "resumed decision must explain missing target"
        );
        require(resume.decision.command.horizontal == 1 && resume.decision.trace.patrol_called, "resume must use existing patrol");
        require(scenario.records().size() == 2, "host must publish exactly one record per update");
    }

    struct Case {
        std::string_view name;
        void (*run)();
    };

    constexpr std::array cases{Case{"notice_stop", notice_stop}, Case{"resume_patrol", resume_patrol}};
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
