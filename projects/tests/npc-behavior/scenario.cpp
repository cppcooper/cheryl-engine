#include "scenario.h"

#include <testing/material-builder.h>

#include <cmath>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace NpcBehavior {
    Tutorial::Character make_actor(glm::vec2 position, float speed) {
        CE::Assets::Sprites::Data data;
        data.definition.grid = {.frame = {16, 16}, .rows = 1, .columns = 2};
        auto sprite = std::make_shared<const CE::Assets::Sprites::Asset>(std::move(data));
        auto builder = CE::Testing::ordinary_materials();
        auto material = builder.values.at(CE::Rendering::Materials::Builtin2D::TexturedStrip);
        return Tutorial::Character(std::move(sprite), std::move(material), position, 1, speed);
    }

    Scenario::Scenario(glm::vec2 position, float speed, float left, float right, float notice_distance, float lane_half_width)
    : actor_(make_actor(position, speed)), controller_(left, right, notice_distance, lane_half_width) {}

    StepRecord Scenario::step(StepInput input) {
        if (count_ == capacity)
            throw std::length_error("Sentry trace capacity exceeded");
        const auto before = actor_.position();
        auto trial_controller = controller_;
        auto trial_actor = actor_;
        const auto decision = trial_controller.read(trial_actor, input.target, input.seconds);
        trial_actor.update(decision.command, input.seconds);
        const auto after = trial_actor.position();
        if (!std::isfinite(after.x) || after.x < decision.trace.left || after.x > decision.trace.right)
            throw std::invalid_argument("Scenario movement leaves its patrol interval");
        const StepRecord record{before, after, decision};

        // No throwing application remains after adoption starts; publish the count last.
        static_assert(std::is_nothrow_copy_assignable_v<Tutorial::Character>);
        static_assert(std::is_nothrow_copy_assignable_v<SentryController>);
        static_assert(std::is_nothrow_copy_assignable_v<StepRecord>);
        static_assert(std::is_nothrow_copy_constructible_v<StepRecord> && std::is_nothrow_move_constructible_v<StepRecord>);
        actor_ = trial_actor;
        controller_ = trial_controller;
        records_[count_] = record;
        ++count_;
        return record;
    }

    std::span<const StepRecord> Scenario::run(std::span<const StepInput> inputs) {
        if (inputs.size() > capacity - count_)
            throw std::length_error("Sentry batch exceeds remaining trace capacity");
        for (const auto& input : inputs)
            static_cast<void>(step(input));
        return records();
    }

    std::span<const StepRecord> Scenario::records() const {
        return {records_.data(), count_};
    }

    const Tutorial::Character& Scenario::actor() const {
        return actor_;
    }
}
