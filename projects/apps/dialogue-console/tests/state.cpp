#include <cheryl/core/game-framework/dialogue.h>

#include "../src/codec.h"

#include <gtest/gtest.h>

namespace G = GateConsole;

TEST(dialogue_console_state, consumer) {
    std::size_t effects = 0;
    G::Encounter encounter(&effects);
    const auto approach = encounter.view().value();
    ASSERT_EQ(approach.choices.size(), 2);
    EXPECT_EQ(approach.text, "A guard waits.");
    EXPECT_EQ(approach.choices[0].text, "Introduce yourself");
    EXPECT_EQ(approach.choices[1].id.value, "silent");
    ASSERT_TRUE(encounter.choose(approach.choices[0].selection));
    const auto saved = encounter.capture().value();
    EXPECT_TRUE(saved.introduced);
    EXPECT_EQ(saved.dialogue.node.value, "gate");
    const auto gate = encounter.view().value();
    ASSERT_TRUE(G::restore_bytes(encounter, G::encode(saved)));
    EXPECT_EQ(encounter.choose(gate.choices[0].selection).error(), G::D::SessionError::WrongIncarnation);
    const auto resumed = encounter.view().value();
    ASSERT_TRUE(resumed.choices[0].available);
    ASSERT_TRUE(encounter.choose(resumed.choices[0].selection));
    EXPECT_EQ(encounter.view()->text, "You enter");
    EXPECT_EQ(effects, 1);
}

TEST(dialogue_console_state, silent_route) {
    std::size_t effects = 0;
    G::Encounter encounter(&effects);
    const auto initial = encounter.view().value();
    ASSERT_TRUE(encounter.choose(initial.choices[1].selection));
    const auto gate = encounter.view().value();
    ASSERT_EQ(gate.choices.size(), 2);
    EXPECT_EQ(gate.text, "The gate is closed.");
    EXPECT_EQ(gate.choices[0].text, "Ask to enter");
    EXPECT_EQ(gate.choices[1].text, "Leave");
    EXPECT_FALSE(gate.choices[0].available);
    const auto before = encounter.capture().value();
    EXPECT_EQ(encounter.choose(gate.choices[0].selection).error(), G::D::SessionError::Unavailable);
    EXPECT_EQ(encounter.choose(initial.choices[0].selection).error(), G::D::SessionError::StaleSelection);
    EXPECT_EQ(encounter.capture().value(), before);
    ASSERT_TRUE(encounter.choose(gate.choices[1].selection));
    EXPECT_EQ(encounter.view()->text, "You depart");
    EXPECT_EQ(effects, 0);
}

TEST(dialogue_console_state, paired_boundaries) {
    std::size_t effects = 0;
    G::Encounter encounter(&effects);
    const auto initial = encounter.capture().value();
    ASSERT_TRUE(G::restore_bytes(encounter, G::encode(initial)));
    ASSERT_TRUE(encounter.choose(encounter.view()->choices[0].selection));
    ASSERT_TRUE(encounter.choose(encounter.view()->choices[0].selection));
    const auto terminal = encounter.capture().value();
    EXPECT_EQ(terminal.dialogue.node.value, "admitted");
    ASSERT_TRUE(G::restore_bytes(encounter, G::encode(terminal)));
    EXPECT_EQ(encounter.view()->kind, G::D::NodeKind::Terminal);
    G::Encounter fresh(&effects);
    ASSERT_TRUE(G::restore_bytes(fresh, G::encode(terminal)));
    EXPECT_EQ(fresh.capture().value(), terminal);
    ASSERT_TRUE(G::restore_bytes(fresh, G::encode(initial)));
    EXPECT_EQ(fresh.capture().value(), initial);
    EXPECT_EQ(effects, 1);
}

TEST(dialogue_console_state, adoption_authority) {
    std::size_t effects = 0;
    G::Encounter encounter(&effects);
    const auto token = encounter.view()->choices[0].selection;
    const auto before = encounter.capture().value();
    auto invalid = before;
    invalid.dialogue.node.value = "missing";
    invalid.introduced = true;
    EXPECT_EQ(encounter.restore(invalid).error(), G::D::SessionError::UnknownNode);
    EXPECT_EQ(encounter.capture().value(), before);
    ASSERT_TRUE(encounter.choose(token));
    const auto gate = encounter.capture().value();
    const auto pass = encounter.view()->choices[0].selection;
    ASSERT_TRUE(encounter.restore(gate));
    EXPECT_EQ(encounter.capture().value(), gate);
    EXPECT_EQ(encounter.choose(pass).error(), G::D::SessionError::WrongIncarnation);
    ASSERT_TRUE(encounter.choose(encounter.view()->choices[0].selection));
    EXPECT_EQ(effects, 1);
}
