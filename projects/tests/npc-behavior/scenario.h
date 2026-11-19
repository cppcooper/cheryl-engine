#pragma once

#include "sentry-controller.h"

#include <array>
#include <cstddef>
#include <span>

namespace NpcBehavior {
    struct StepInput {
        std::optional<glm::vec2> target;
        double seconds = 0.5;
    };

    struct StepRecord {
        glm::vec2 before{};
        glm::vec2 after{};
        Decision decision{};
    };

    [[nodiscard]] Tutorial::Character make_actor(glm::vec2 position = {80, 220}, float speed = 80);

    // The host owns one actual Character, one controller and at most 32 inline records.
    class Scenario final {
    public:
        static constexpr std::size_t capacity = 32;

    private:
        Tutorial::Character actor_;
        SentryController controller_;
        std::array<StepRecord, capacity> records_{};
        std::size_t count_ = 0;

    public:
        explicit Scenario(
            glm::vec2 position = {80, 220},
            float speed = 80,
            float left = 80,
            float right = 500,
            float notice_distance = 120,
            float lane_half_width = 16
        );

        [[nodiscard]] StepRecord step(StepInput input);
        // Appends; oversize requests reject before any execution. Other failures retain successful prior steps.
        [[nodiscard]] std::span<const StepRecord> run(std::span<const StepInput> inputs);
        // Borrowed view: copy a StepRecord to retain it beyond this host's lifetime.
        [[nodiscard]] std::span<const StepRecord> records() const;
        [[nodiscard]] const Tutorial::Character& actor() const;
    };
}
