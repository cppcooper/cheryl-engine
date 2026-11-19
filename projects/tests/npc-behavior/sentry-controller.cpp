#include "sentry-controller.h"

#include <cmath>
#include <stdexcept>
#include <type_traits>

namespace NpcBehavior {
    SentryController::SentryController(float left, float right, float notice_distance, float lane_half_width)
    : left_(left), right_(right), notice_distance_(notice_distance), lane_half_width_(lane_half_width), patrol_(left, right) {
        if (!std::isfinite(notice_distance) || notice_distance < 0 || !std::isfinite(lane_half_width) || lane_half_width < 0)
            throw std::invalid_argument("Sentry requires finite nonnegative proximity bounds");
    }

    Decision SentryController::read(const Tutorial::Character& actor, std::optional<glm::vec2> target, double seconds) {
        const auto position = actor.position();
        const auto speed = actor.speed();
        if (!std::isfinite(position.x) || !std::isfinite(position.y) || position.x < left_ || position.x > right_ ||
            !std::isfinite(speed) || speed <= 0)
            throw std::invalid_argument("Sentry requires a finite actor inside its patrol interval with positive speed");
        if (!std::isfinite(seconds) || seconds < 0 || !std::isfinite(static_cast<double>(speed) * seconds))
            throw std::invalid_argument("Sentry requires finite nonnegative time and a finite prospective step");
        if (target && (!std::isfinite(target->x) || !std::isfinite(target->y)))
            throw std::invalid_argument("Sentry requires finite target coordinates, including at zero time");

        Decision decision{};
        auto& trace = decision.trace;
        trace.actor_position = position;
        trace.actor_speed = speed;
        trace.seconds = seconds;
        trace.target = target;
        trace.left = left_;
        trace.right = right_;
        trace.notice_distance = notice_distance_;
        trace.lane_half_width = lane_half_width_;
        if (seconds == 0)
            return decision;

        trace.action = Action::Patrol;
        trace.reason = Reason::NoTarget;
        if (target) {
            trace.sensor_evaluated = true;
            trace.absolute_dx = std::abs(static_cast<double>(target->x) - static_cast<double>(position.x));
            trace.absolute_dy = std::abs(static_cast<double>(target->y) - static_cast<double>(position.y));
            trace.in_lane = *trace.absolute_dy <= static_cast<double>(lane_half_width_);
            trace.in_range = *trace.absolute_dx <= static_cast<double>(notice_distance_);
            trace.detected = trace.in_lane && trace.in_range;
            if (!trace.in_lane)
                trace.reason = Reason::OutOfLane;
            else if (!trace.in_range)
                trace.reason = Reason::OutOfRange;
            else {
                trace.action = Action::Watch;
                trace.reason = Reason::TargetDetected;
                return decision;
            }
        }

        auto trial_patrol = patrol_;
        decision.command = trial_patrol.read(actor, seconds);
        // Use Character's actual clamped expression and float conversion, not a distance estimate.
        auto trial_actor = actor;
        trial_actor.update(decision.command, seconds);
        const auto next = trial_actor.position();
        if (!std::isfinite(next.x) || next.x < left_ || next.x > right_)
            throw std::invalid_argument("Sentry movement leaves its patrol interval");
        trace.patrol_called = true;
        trace.horizontal_command = decision.command.horizontal;
        static_assert(std::is_nothrow_copy_assignable_v<Tutorial::PatrolController>);
        static_assert(std::is_nothrow_copy_constructible_v<Decision> && std::is_nothrow_move_constructible_v<Decision>);
        patrol_ = trial_patrol;
        return decision;
    }
}
