#pragma once

#include <contract/fixture.h>
#include <cheryl/story/coordinator.h>
#include "consumer.h"

namespace ActorFixture {
    namespace Story = CE::Story;
    struct World {
        std::unique_ptr<Story::ActorCoordinator> host;
        Story::ActorEndpoint endpoint;
        Story::KnownPersonRef contact;
        Story::KnownObjectRef object;
    };
    World response_world(bool hidden = false, Story::Limits = {});
    Story::ActorOffer response_offer(World&);
    Story::ActorChoice choice(const Story::ActorOffer&);
    Story::ActorView canonical(Story::ActorView);
    Story::ActorReceipt canonical(Story::ActorReceipt);
    void hidden_transfer(Story::Family&);
    Story::Result scheduled(Story::Family&, Story::PersonId, Story::StoryTime, Story::Action);
}
