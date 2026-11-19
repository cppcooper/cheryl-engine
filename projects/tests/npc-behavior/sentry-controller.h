#pragma once

#include "../../apps/tutorial-game/src/patrol-controller.h"

#include <optional>

namespace NpcBehavior {
    enum class Action { Hold, Watch, Patrol };
    enum class Reason { ZeroTime, NoTarget, OutOfLane, OutOfRange, TargetDetected };

    struct Trace {
        glm::vec2 actor_position{};
        float actor_speed = 0;
        double seconds = 0;
        std::optional<glm::vec2> target;
        float left = 0;
        float right = 0;
        float notice_distance = 0;
        float lane_half_width = 0;
        bool sensor_evaluated = false;
        std::optional<double> absolute_dx;
        std::optional<double> absolute_dy;
        bool in_lane = false;
        bool in_range = false;
        bool detected = false;
        bool patrol_called = false;
        Action action = Action::Hold;
        Reason reason = Reason::ZeroTime;
        double horizontal_command = 0;
    };

    struct Decision {
        Tutorial::CharacterCommand command;
        Trace trace;
    };

    // Private policy for one unobstructed corridor: rectangular proximity, not line-of-sight.
    class SentryController final {
        float left_;
        float right_;
        float notice_distance_;
        float lane_half_width_;
        Tutorial::PatrolController patrol_;

    public:
        SentryController(float left = 80, float right = 500, float notice_distance = 120, float lane_half_width = 16);

        // Borrows only for this call. Validation precedes patrol mutation; the caller alone applies movement.
        [[nodiscard]] Decision read(const Tutorial::Character& actor, std::optional<glm::vec2> target, double seconds);
    };
}
