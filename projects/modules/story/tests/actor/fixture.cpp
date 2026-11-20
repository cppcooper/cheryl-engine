#include "fixture.h"
#include "../../src/actor/state.h"
#include <stdexcept>

namespace ActorFixture {
    namespace {
        void require(bool condition) {
            if (!condition) { throw std::runtime_error("actor fixture prerequisite failed"); }
        }
        template<class Tag> void local(Story::ActorRef<Tag>& reference) {
            reference = Story::Detail::ActorAccess::canonical(reference);
        }
        void local(Story::ActorIntent& intent) {
            local(intent.person); local(intent.object); local(intent.promise); local(intent.account);
        }
    }
    Story::Result scheduled(Story::Family& owner, Story::PersonId person, Story::StoryTime at, Story::Action action) {
        const auto actor = owner.actor(person);
        const auto attempt = owner.allocate_attempt(actor);
        const auto order = owner.order_at(at);
        require(attempt.has_value() && order.has_value());
        return owner.apply(actor, {*attempt, *order, std::move(action)});
    }
    void hidden_transfer(Story::Family& owner) {
        require(static_cast<bool>(scheduled(owner, StoryFixture::oren, {3},
            Story::ConsentTransfer{StoryFixture::substitute, StoryFixture::sera, 1})));
        require(static_cast<bool>(scheduled(owner, StoryFixture::sera, {4},
            Story::Transfer{StoryFixture::substitute, StoryFixture::oren, 2})));
    }
    World response_world(bool hidden, Story::Limits limits) {
        auto family = StoryFixture::family(limits);
        StoryFixture::promise(*family);
        if (hidden) { hidden_transfer(*family); }
        auto created = Story::ActorCoordinator::create(std::move(family), {5});
        require(created.has_value());
        auto host = std::move(*created);
        const auto endpoint = host->open(StoryFixture::liora);
        require(endpoint.has_value());
        const auto contact = host->identify(*endpoint, StoryFixture::sera, Story::Text::make("Known contact").value());
        const auto object = host->identify(*endpoint, StoryFixture::box, Story::Text::make("Known object").value());
        require(contact.has_value() && object.has_value());
        return {std::move(host), *endpoint, *contact, *object};
    }
    Story::ActorOffer response_offer(World& world) {
        const auto view = world.endpoint.view();
        require(view.has_value() && view->account_count >= 2);
        Story::ActorIntent intent;
        intent.kind = Story::IntentKind::Respond;
        intent.person = world.contact;
        intent.account = view->accounts[1].reference;
        intent.attitude_delta = -1;
        const auto offered = world.host->offer(world.endpoint, intent, Story::Text::make("React privately").value());
        require(offered.has_value());
        return *offered;
    }
    Story::ActorChoice choice(const Story::ActorOffer& offer) { return {offer.reference, offer.intent}; }
    Story::ActorView canonical(Story::ActorView view) {
        local(view.self);
        for (auto& person : view.people) { local(person.reference); }
        for (auto& object : view.objects) { local(object.reference); }
        for (auto& account : view.accounts) {
            local(account.reference); local(account.attribution);
            local(account.fact.subject); local(account.fact.other); local(account.fact.object);
            local(account.fact.promise); local(account.fact.partnership);
        }
        for (auto& offer : view.offers) { local(offer.reference); local(offer.intent); }
        return view;
    }
    Story::ActorReceipt canonical(Story::ActorReceipt receipt) { local(receipt.promise); return receipt; }
}
