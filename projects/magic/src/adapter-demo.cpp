#include <magic/runtime.h>
#include <magic/reference-host.h>

#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>

namespace Party {
    using namespace Magic;
    namespace C = Magic::Contact;
    struct Member {
        std::array<std::uint32_t, 2> name;
        std::uint64_t generation;
        std::uint32_t vitality, focus;
        bool living = true;
        bool operator==(const Member&) const = default;
    };
    struct State {
        std::array<Member, 2> members{{{{7, 91}, 4, 30, 21}, {{8, 12}, 9, 35, 0}}};
        std::uint64_t revision = 1, schema = 1, casts = 0;
        bool permission = true, willing = true, touching = true, eligible = true;
        bool operator==(const State&) const = default;
    };
    // The application owns this aggregate. Logical names are pairs, not slots;
    // the adapter explicitly resolves them and generation on every inspection.
    struct Aggregate {
        State state;
        Catalog catalog;
        Aggregate() { catalog.operations = {{Primitive::Heal, "game.heal"}}; }
    };
    C::Entity identity(const Member& member) {
        return {42, (static_cast<std::uint64_t>(member.name[0]) << 32) | member.name[1], member.generation};
    }
    C::Request request(const Aggregate& party) {
        return {identity(party.state.members[0]), identity(party.state.members[1])};
    }
    class Adapter final : public C::WorldAdapter {
        Aggregate& party_;
        class Transaction final : public C::Candidate {
            Aggregate& owner_;
            State before_, next_;
        public:
            Transaction(Aggregate& owner, State before, State next) : owner_(owner), before_(before), next_(next) {}
            std::expected<void, C::Error> adopt() noexcept override {
                if (owner_.state != before_)
                    return std::unexpected(C::Error::StaleTicket);
                owner_.state = next_;
                return {};
            }
        };
    public:
        bool fail_preparation = false, reject_late = false, throw_preparation = false;
        explicit Adapter(Aggregate& party) : party_(party) {}
        const Catalog& catalog() const noexcept override { return party_.catalog; }
        std::expected<C::Facts, C::Error> inspect(C::Request value) const override {
            const Member* caster = nullptr;
            const Member* recipient = nullptr;
            for (const auto& member : party_.state.members) {
                if (identity(member) == value.caster) caster = &member;
                if (identity(member) == value.recipient) recipient = &member;
            }
            if (!caster || !recipient)
                return std::unexpected(C::Error::MissingEntity);
            const auto& s = party_.state;
            return C::Facts{s.revision, s.schema, recipient->vitality, 40, caster->focus, 3,
                caster->living && caster->vitality != 0, s.permission,
                recipient->living && recipient->vitality != 0, s.eligible, s.willing, s.touching};
        }
        std::expected<std::unique_ptr<C::Candidate>, C::Error> candidate(const C::Change& change) override {
            if (throw_preparation)
                throw std::bad_alloc();
            if (fail_preparation)
                return std::unexpected(C::Error::Preparation);
            auto live = inspect(change.request);
            if (!live || *live != change.before)
                return std::unexpected(C::Error::StaleTicket);
            if (party_.state.revision == std::numeric_limits<std::uint64_t>::max() ||
                party_.state.casts == std::numeric_limits<std::uint64_t>::max())
                return std::unexpected(C::Error::RevisionLimit);
            auto next = party_.state;
            for (auto& member : next.members) {
                if (identity(member) == change.request.caster) member.focus = change.resource;
                if (identity(member) == change.request.recipient) member.vitality = change.health;
            }
            ++next.revision;
            ++next.casts;
            std::unique_ptr<C::Candidate> result = std::make_unique<Transaction>(party_, party_.state, next);
            // Models a serialized owner edit between candidate creation and commit.
            if (reject_late) {
                ++party_.state.revision;
                party_.state.members[0].vitality = 29;
            }
            return result;
        }
    };
    template<class T, class E> T take(std::expected<T, E> value) {
        if (!value) throw std::runtime_error("consumer operation refused");
        return std::move(*value);
    }
    PlanHandle spell() {
        std::ifstream input(std::string(MAGIC_FIXTURE_DIR) + "/S1.verse");
        if (!input) throw std::runtime_error("S1 fixture unavailable");
        std::string source(std::istreambuf_iterator<char>(input), {});
        Catalog catalog;
        catalog.operations = {{Primitive::Heal, "game.heal"}};
        return take(Magic::compile(source, catalog));
    }
}

#ifndef MAGIC_ADAPTER_NO_MAIN
int main() {
    try {
        const auto plan = Party::spell(); // compile S1 exactly once; same handle for both owners
        auto reference = Party::take(Magic::Reference::ReferenceHost::create(Party::Aggregate{}.catalog));
        const auto caster = Party::take(reference->add_actor({{}, 100, 100, 100, 1000, false, true, true}));
        const auto target = Party::take(reference->add_actor({{}, 50, 100, 0, 1000, true, true, false}));
        const auto reference_result = Party::take(reference->release(Party::take(reference->prepare(plan, {caster, target, {}}))));
        if (reference->snapshot().actors[target.slot].data.health != 62 ||
            reference->snapshot().actors[caster.slot].data.mana != 90)
            throw std::runtime_error("reference literal mismatch");
        Party::Aggregate party;
        Party::Adapter adapter(party);
        Magic::Contact::Runtime runtime(adapter);
        const auto ticket = Party::take(runtime.prepare(plan, Party::request(party)));
        const auto report = Party::take(runtime.release(ticket));
        if (party.state.members[1].vitality != 40 || party.state.members[0].focus != 18 || party.state.casts != 1)
            throw std::runtime_error("party literal mismatch");
        const auto stale = Party::take(runtime.prepare(plan, Party::request(party)));
        party.state.members[1].vitality = 11;
        ++party.state.revision;
        if (runtime.release(stale) || party.state.members[1].vitality != 11 || party.state.members[0].focus != 18)
            throw std::runtime_error("owner edit not preserved");
        std::cout << "reference health=62 mana=90; party health=40 focus=18 casts=1; owner edit health=11 preserved\n";
        if (report->health != 40 || reference_result->after.actors[target.slot].data.health != 62)
            throw std::runtime_error("owned report mismatch");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
#endif
