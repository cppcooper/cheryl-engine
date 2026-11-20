#include "state.h"

namespace CE::Story::Detail {
    namespace {
        template<class Tag, class Raw, std::size_t Capacity>
        ActorRef<Tag> map(std::array<Binding<Raw>, Capacity>& bindings, std::size_t& count,
            std::uint64_t scope, Raw raw, bool mention, bool actionable = false) {
            if (!raw) { return {}; }
            if (!mention) {
                for (std::size_t index = 0; index < count; ++index) {
                    if (!bindings[index].mention && bindings[index].raw == raw) {
                        bindings[index].actionable = bindings[index].actionable || actionable;
                        return ActorAccess::ref<Tag>(scope, index + 1);
                    }
                }
            }
            if (count == Capacity) { throw Error::Capacity; }
            bindings[count] = {raw, actionable, mention};
            return ActorAccess::ref<Tag>(scope, ++count);
        }
        ActorFactKind kind(EpisodeKind value) {
            switch (value) {
#define STORY_ACTOR_KIND(Name) case EpisodeKind::Name: return ActorFactKind::Name;
                STORY_ACTOR_KIND(PromiseOffered) STORY_ACTOR_KIND(PromiseAccepted) STORY_ACTOR_KIND(PromiseDeclined)
                STORY_ACTOR_KIND(ReleaseRequested) STORY_ACTOR_KIND(ReleaseAccepted) STORY_ACTOR_KIND(ReleaseDeclined)
                STORY_ACTOR_KIND(Deadline) STORY_ACTOR_KIND(TransferConsented) STORY_ACTOR_KIND(Transferred)
                STORY_ACTOR_KIND(Perceived) STORY_ACTOR_KIND(Told) STORY_ACTOR_KIND(Response)
                STORY_ACTOR_KIND(Introduced) STORY_ACTOR_KIND(PartnershipProposed) STORY_ACTOR_KIND(PartnershipAccepted)
                STORY_ACTOR_KIND(PartnershipDeclined) STORY_ACTOR_KIND(PartnershipEnded) STORY_ACTOR_KIND(Born)
                STORY_ACTOR_KIND(Died) STORY_ACTOR_KIND(Left) STORY_ACTOR_KIND(Rejoined)
                STORY_ACTOR_KIND(CommitmentFailed) STORY_ACTOR_KIND(PartnershipCancelled)
#undef STORY_ACTOR_KIND
            }
            throw Error::Unsupported;
        }
    }
    Error project(Family& family, ActorState& state) {
        std::array<Knowledge, 64> acquired{};
        const auto count = family.knowledge(family.actor(state.person), acquired);
        if (!count) { return count.error(); }
        if (*count < state.view.account_count) { return Error::Conflict; }
        if (*count == state.view.account_count) { return Error::None; }
        auto candidate = state;
        try {
            for (std::size_t index = candidate.view.account_count; index < *count; ++index) {
                const auto& source = acquired[index];
                if (source.local_sequence != index + 1) { return Error::Conflict; }
                const auto mention = source.evidence == Evidence::Attributed;
                auto person = [&](PersonId raw) {
                    return map<ActorPersonTag>(candidate.people, candidate.view_people_count,
                        candidate.scope, raw, mention);
                };
                ActorAccount account;
                account.reference = ActorAccess::ref<ActorAccountTag>(candidate.scope, index + 1);
                account.fact.kind = kind(source.proposition.kind);
                account.fact.subject = person(source.proposition.subject);
                account.fact.other = !mention && source.proposition.other == source.proposition.subject
                    ? account.fact.subject : person(source.proposition.other);
                account.fact.object = map<ActorObjectTag>(candidate.objects, candidate.view_objects_count,
                    candidate.scope, source.proposition.object, mention);
                account.fact.promise = map<ActorPromiseTag>(candidate.promises, candidate.promise_count,
                    candidate.scope, source.proposition.commitment, mention, !mention &&
                        (source.proposition.subject == state.person || source.proposition.other == state.person));
                account.fact.partnership = map<ActorPartnershipTag>(candidate.partnerships, candidate.partnership_count,
                    candidate.scope, source.proposition.partnership, mention);
                account.attribution = map<ActorPersonTag>(candidate.people, candidate.view_people_count,
                    candidate.scope, source.attributed_to, false);
                if (!mention && source.proposition.commitment &&
                    (source.proposition.subject == state.person || source.proposition.other == state.person)) {
                    const auto promise = family.commitment(source.proposition.commitment);
                    if (!promise || (promise->debtor != state.person && promise->creditor != state.person)) {
                        return Error::Authority;
                    }
                    account.fact.due = promise->due;
                }
                account.evidence = mention ? ActorEvidence::Attributed : ActorEvidence::Observed;
                account.acquired = source.acquired;
                candidate.accounts[index] = source.local_sequence;
                candidate.view.accounts[index] = account;
                ++candidate.view.account_count;
            }
            for (std::size_t index = 0; index < candidate.view_people_count; ++index) {
                auto& person = candidate.view.people[index];
                person.reference = ActorAccess::ref<ActorPersonTag>(candidate.scope, index + 1);
                person.identified = candidate.people[index].actionable;
            }
            for (std::size_t index = 0; index < candidate.view_objects_count; ++index) {
                auto& object = candidate.view.objects[index];
                object.reference = ActorAccess::ref<ActorObjectTag>(candidate.scope, index + 1);
                object.identified = candidate.objects[index].actionable;
            }
        } catch (Error error) { return error; }
        candidate.view.person_count = candidate.view_people_count;
        candidate.view.object_count = candidate.view_objects_count;
        candidate.view.total_accounts = candidate.view.account_count;
        candidate.view.cursor = candidate.view.account_count;
        state = std::move(candidate);
        return Error::None;
    }
}
