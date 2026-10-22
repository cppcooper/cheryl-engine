#include "../src/character.h"
#include "../src/patrol-controller.h"
#include "../src/player-controller.h"

#include <gtest/gtest.h>
#include <testing/material-builder.h>

#include <limits>
#include <memory>
#include <optional>

namespace {
    Tutorial::Character character(float x = 80, float speed = 80) {
        CE::Assets::Sprites::Data data;
        data.definition.grid = {.frame = {16, 16}, .rows = 1, .columns = 2};
        // Movement tests need the shared definition/material, without native uploads.
        auto sprite = std::make_shared<const CE::Assets::Sprites::Asset>(std::move(data));
        auto builder = CE::Testing::ordinary_materials();
        auto material = builder.values.at(CE::Rendering::Materials::Builtin2D::TexturedStrip);
        return Tutorial::Character(std::move(sprite), std::move(material), {x, 100}, 0, speed);
    }
}

TEST(tutorial_game, controllers) {
    CE::Input::Actions::Bindings input;
    const auto endpoint = input.action_contexts();
    Tutorial::PlayerController first(endpoint.attach(Tutorial::PlayerController::keyboard(1)));
    Tutorial::PlayerController second(
        endpoint.attach(Tutorial::PlayerController::keyboard(1, CE::Input::KeyboardKey::J, CE::Input::KeyboardKey::L))
    );
    const auto baseline = input.action_snapshot();
    input.on_button({1, CE::Input::KeyboardKey::Right}, true);
    const auto right = input.publish_actions();
    const CE::Input::Tick tick(baseline, {right});
    EXPECT_FLOAT_EQ(first.read(tick).horizontal, 1);
    EXPECT_FLOAT_EQ(second.read(tick).horizontal, 0);

    input.on_button({1, CE::Input::KeyboardKey::L}, true);
    input.on_button({1, CE::Input::KeyboardKey::Left}, true);
    const CE::Input::Tick both(right, {input.publish_actions()});
    EXPECT_FLOAT_EQ(first.read(both).horizontal, 0);
    EXPECT_FLOAT_EQ(second.read(both).horizontal, 1);
}

TEST(tutorial_game, rejoin) {
    CE::Input::Actions::Bindings input;
    const auto endpoint = input.action_contexts();
    const auto definition = Tutorial::PlayerController::keyboard(1);
    std::optional<Tutorial::PlayerController> controller;
    controller.emplace(endpoint.attach(definition));
    const auto baseline = input.action_snapshot();
    input.on_button({1, CE::Input::KeyboardKey::Right}, true);
    const auto pressed = input.publish_actions();
    const CE::Input::Tick prior(baseline, {pressed});
    EXPECT_FLOAT_EQ(controller->read(prior).horizontal, 1);
    controller.reset();
    const auto removed = input.publish_actions();
    controller.emplace(endpoint.attach(definition));
    const CE::Input::Tick rejoined(removed, {input.publish_actions()});
    EXPECT_FLOAT_EQ(controller->read(rejoined).horizontal, 1);
}

TEST(tutorial_game, movement) {
    auto actor = character();
    actor.update({1}, 0.5);
    EXPECT_FLOAT_EQ(actor.position().x, 120);
    actor.update({-1}, 0.25);
    EXPECT_FLOAT_EQ(actor.position().x, 100);
    actor.update({}, 1);
    EXPECT_FLOAT_EQ(actor.position().x, 100);
    EXPECT_FLOAT_EQ(actor.position().y, 100);
}

TEST(tutorial_game, patrol) {
    auto actor = character();
    Tutorial::PatrolController controller(80, 500);
    actor.update(controller.read(actor, 10), 10);
    EXPECT_FLOAT_EQ(actor.position().x, 500);
    actor.update(controller.read(actor, 10), 10);
    EXPECT_FLOAT_EQ(actor.position().x, 80);
    actor.update(controller.read(actor, 0), 0);
    EXPECT_FLOAT_EQ(actor.position().x, 80);
}

TEST(tutorial_game, invalid) {
    auto actor = character();
    EXPECT_THROW(actor.update({std::numeric_limits<float>::quiet_NaN()}, 1), std::invalid_argument);
    EXPECT_THROW(actor.update({1}, -1), std::invalid_argument);
    EXPECT_THROW(actor.update({1}, std::numeric_limits<double>::max()), std::invalid_argument);
    EXPECT_FLOAT_EQ(actor.position().x, 80);
    EXPECT_THROW(static_cast<void>(Tutorial::PatrolController(500, 80)), std::invalid_argument);
}
