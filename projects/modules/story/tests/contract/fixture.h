#pragma once

#include <cheryl/story/actions.h>

namespace StoryFixture {
    namespace Story = CE::Story;
    inline constexpr Story::TimelineId timeline{41};
    inline constexpr Story::PersonId sera{timeline, 100};
    inline constexpr Story::PersonId liora{timeline, 400};
    inline constexpr Story::PersonId oren{timeline, 900};
    inline constexpr Story::ObjectId box{timeline, 71};
    inline constexpr Story::ObjectId substitute{timeline, 72};
    struct Inputs {
        std::array<Story::Person, 3> people;
        std::array<Story::ObjectState, 2> objects;
        std::array<Story::Contact, 2> contacts;
        Inputs();
        Story::Seed seed() const;
    };
    std::unique_ptr<Story::Family> family(Story::Limits limits = {});
    Story::Request request(std::uint64_t attempt, std::int64_t time, Story::Action action);
    Story::CommitmentId promise(Story::Family&);
    struct Trace {
        Story::Result offered;
        Story::Result accepted;
        Story::Result consented;
        Story::Result transferred;
        Story::Result duplicate;
        Story::Result refusal;
        Story::Result told;
        std::size_t episodes{};
        std::size_t acquisitions{};
        std::array<Story::Knowledge, 8> oren_accounts{};
        std::size_t oren_count{};
    };
    Trace baseline(Story::Family&);
}
