#pragma once

#include "character-command.h"

#include <cheryl/core/controls/action-context.h>
#include <cheryl/core/controls/button-codes.h>
#include <cheryl/core/controls/tick-input.h>

#include <utility>

namespace Tutorial {
    class PlayerController final {
        static constexpr CE::Input::Actions::Id left_{1};
        static constexpr CE::Input::Actions::Id right_{2};
        CE::Input::Actions::Context input_;

    public:
        explicit PlayerController(CE::Input::Actions::Context input)
        : input_(std::move(input)) {}

        [[nodiscard]] static CE::Input::Actions::ContextDefinition keyboard(
            CE::Input::DeviceId device,
            CE::Input::KeyboardKey left = CE::Input::KeyboardKey::Left,
            CE::Input::KeyboardKey right = CE::Input::KeyboardKey::Right
        ) {
            CE::Input::Actions::ContextDefinition definition;
            definition.buttons.push_back({{{{device, left}}}, left_});
            definition.buttons.push_back({{{{device, right}}}, right_});
            return definition;
        }

        [[nodiscard]] CharacterCommand read(const CE::Input::Tick& tick) const {
            const auto actions = tick.actions(input_);
            return {(actions.button(right_).held() ? 1.0f : 0.0f) - (actions.button(left_).held() ? 1.0f : 0.0f)};
        }
    };
}
