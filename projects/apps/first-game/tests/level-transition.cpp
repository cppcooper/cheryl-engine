#include "first-game.h"
#include "level-transition.h"

#include <gtest/gtest.h>

#include <stdexcept>

TEST(first_game_levels, room_specs) {
    const auto courtyard = FirstGame::Levels::room_spec(FirstGame::Levels::LevelId::Courtyard);
    const auto study = FirstGame::Levels::room_spec(FirstGame::Levels::LevelId::Study);
    EXPECT_EQ(courtyard.label, "Courtyard");
    EXPECT_EQ(study.label, "Study");
    EXPECT_FLOAT_EQ(courtyard.spawn.x, 0.5f);
    EXPECT_FLOAT_EQ(study.spawn.x, 0.75f);
    EXPECT_FLOAT_EQ(courtyard.spawn.y, 0.5f);
    EXPECT_FLOAT_EQ(study.spawn.y, 0.5f);
    EXPECT_EQ(courtyard.cell, 0u);
    EXPECT_EQ(study.cell, 1u);
    EXPECT_THROW(static_cast<void>(FirstGame::Levels::room_spec(static_cast<FirstGame::Levels::LevelId>(42))), std::invalid_argument);
}
