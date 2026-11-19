#include "scenario.h"

#include <exception>
#include <iostream>
#include <stdexcept>

int main() {
    try {
        NpcBehavior::Scenario scenario;
        const auto watch = scenario.step({glm::vec2{160, 220}, 0.5});
        const auto resume = scenario.step({std::nullopt, 0.5});
        if (watch.decision.trace.action != NpcBehavior::Action::Watch || watch.decision.command.horizontal != 0 || watch.after.x != 80 ||
            watch.after.y != 220 || resume.decision.trace.action != NpcBehavior::Action::Patrol ||
            resume.decision.command.horizontal != 1 || resume.after.x != 120 || resume.after.y != 220)
            throw std::runtime_error("Real Character notice/resume consequence mismatch");
        for (const auto& record : scenario.records()) {
            const auto& trace = record.decision.trace;
            std::cout << "actor=(" << trace.actor_position.x << ',' << trace.actor_position.y << ") speed=" << trace.actor_speed
                      << " seconds=" << trace.seconds << " action=" << (trace.action == NpcBehavior::Action::Watch ? "Watch" : "Patrol")
                      << " reason=" << (trace.reason == NpcBehavior::Reason::TargetDetected ? "TargetDetected" : "NoTarget")
                      << " command=" << record.decision.command.horizontal << " before=(" << record.before.x << ',' << record.before.y
                      << ") after=(" << record.after.x << ',' << record.after.y << ")\n";
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "npc_behavior.consumer: " << error.what() << '\n';
        return 1;
    }
}
