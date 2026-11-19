#include "scenario.h"

#include <testing/material-builder.h>

#include <memory>
#include <stdexcept>
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
        const auto decision = controller_.read(actor_, input.target, input.seconds);
        actor_.update(decision.command, input.seconds);
        const StepRecord record{before, actor_.position(), decision};
        records_[count_++] = record;
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
