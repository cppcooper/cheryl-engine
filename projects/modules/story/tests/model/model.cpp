#include <contract/fixture.h>
#include <gtest/gtest.h>

using namespace StoryFixture;

TEST(Storymodel, seed_depth_and_owned_values) {
    auto owner = family();
    EXPECT_EQ(owner->person(sera)->depth, 0);
    EXPECT_EQ(owner->person(liora)->depth, 1);
    EXPECT_EQ(owner->person(oren)->parents[1], Story::PersonId{});
    EXPECT_EQ(owner->person(liora)->name.view(), "Liora");
    EXPECT_EQ(owner->counters().validation_passes, 1);
}

TEST(Storymodel, missing_parent) {
    Inputs inputs;
    inputs.people[1].parents[0] = {timeline, 9999};
    const auto seed = inputs.seed();
    const auto result = Story::Family::create(seed);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), Story::Error::Missing);
}

TEST(Storymodel, self_parent) {
    Inputs inputs;
    inputs.people[1].parents[0] = liora;
    const auto result = Story::Family::create(inputs.seed());
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), Story::Error::Cycle);
}

TEST(Storymodel, future_death_not_an_occurrence) {
    Inputs inputs;
    inputs.people[0].died = Story::StoryTime{10};
    const auto result = Story::Family::create(inputs.seed());
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), Story::Error::InvalidSeed);
}

TEST(Storymodel, social_cycle_is_not_ancestry) {
    Inputs inputs;
    const std::array<Story::Contact, 3> contacts{{{sera, liora}, {liora, oren}, {oren, sera}}};
    auto seed = inputs.seed();
    seed.contacts = contacts;
    const auto result = Story::Family::create(seed);
    ASSERT_TRUE(result);
}

TEST(Storymodel, incoming_does_not_invent_depth) {
    Inputs inputs;
    inputs.people[2].parents = {};
    inputs.people[2].lineage = Story::Lineage::Incoming;
    const auto result = Story::Family::create(inputs.seed());
    ASSERT_TRUE(result);
    EXPECT_FALSE((*result)->person(oren)->depth);
}

TEST(Storymodel, checked_limits_and_time) {
    EXPECT_FALSE(Story::StoryTime::add({std::numeric_limits<std::int64_t>::max()}, 1));
    EXPECT_FALSE(Story::StoryTime::add({std::numeric_limits<std::int64_t>::min()}, -1));
    EXPECT_EQ(Story::StoryTime::add({-10}, 4)->value, -6);
    EXPECT_FALSE(Story::Text::make(std::string(97, 'x')));
    Inputs inputs;
    Story::Limits limits;
    limits.people = std::numeric_limits<std::size_t>::max();
    const auto result = Story::Family::create(inputs.seed(), limits);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error(), Story::Error::Bytes);
}
