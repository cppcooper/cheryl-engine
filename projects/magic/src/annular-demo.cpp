#include <magic/learning.h>
#include <magic/reference-host.h>

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
    using namespace Magic;
    template <class T, class E> T value(std::expected<T, E> r) {
        if (!r)
            throw std::runtime_error(std::string(error_name(r.error().code)));
        return std::move(*r);
    }
    template <class E> void done(std::expected<void, E> r) {
        if (!r)
            throw std::runtime_error(std::string(error_name(r.error().code)));
    }
    void require(bool b, std::string_view why) {
        if (!b)
            throw std::runtime_error(std::string(why));
    }
    std::string read(std::string_view file) {
        std::ifstream in(std::string(MAGIC_FIXTURE_DIR) + "/" + std::string(file));
        require(in.good(), "authored fixture opens");
        std::string s(std::istreambuf_iterator<char>(in), {});
        if (s.ends_with('\n'))
            s.pop_back();
        return s;
    }
}
int main() {
    using namespace Magic;
    using namespace Magic::Learning;
    using namespace Magic::Reference;
    try {
        Catalog schema;
        schema.operations = {{Primitive::Heal, "game.heal"}, {Primitive::Fire, "game.fire"}, {Primitive::Slow, "game.slow"}};
        Checkpoint saved;
        EntryHandle disk;
        Ticket old_ticket;
        ActorId protected_centre, entered, left_inner, entered_outer, removed, ineligible;
        {
            auto learner = value(Learner::create(all_rules & ~rule_bit(Rule::AnnularTarget)));
            auto book = value(Spellbook::create());
            done(book->add("burst", "Keep the centre safe"));
            auto before = value(book->edit("burst", read("S2.verse"), "Invented school words; ordinary disk.", schema, learner));
            require(!before->problem(), "accepted disk before lesson");
            disk = before->accepted();
            auto unlearned = value(book->edit("burst", read("m5/burst.verse"), "", schema, learner));
            require(
                unlearned->problem() && unlearned->problem()->missing == rule_bit(Rule::AnnularTarget),
                "recognized annulus still needs lesson"
            );
            auto original = read("m5/lesson.verse"), gloss = read("m5/lesson.common"), practice = read("m5/practice.verse"),
                 practice_gloss = read("m5/practice.common");
            auto lesson = value(make_lesson(
                {"protected-centre", "target.annulus@1",
                 "Invented Mend/Cinder, explicit shared targeting bridge; exclude the inclusive inner disk.", original, gloss, practice,
                 practice_gloss},
                schema
            ));
            require(lesson->target_alignments().size() == 1, "actual checked target alignment");
            std::cout << "Taught " << rule_key(lesson->rule()) << ": " << lesson->target_alignments()[0].gloss << '\n';
            require(value(learner.learn(lesson)), "learns modifier only");
            auto after = value(book->edit("burst", read("m5/burst.verse"), "Protect radius100mm, affect through250mm.", schema, learner));
            require(!after->problem() && after->accepted()->revision() == 2, "edited annular revision");
            require(after->accepted()->common() == read("m5/burst.common"), "exact independent Common readback");
            std::cout << after->accepted()->common() << '\n';
            auto host = value(ReferenceHost::create(schema));
            auto caster = value(host->add_actor({{1600, -500}, 100, 100, 100, 1000, false, true, true}));
            protected_centre = value(host->add_actor({{1000, -500}, 50, 100, 0, 1000, true, true, false}));
            entered = value(host->add_actor({{1100, -500}, 50, 100, 0, 1000, true, true, false}));
            left_inner = value(host->add_actor({{1101, -500}, 50, 100, 0, 1000, true, true, false}));
            entered_outer = value(host->add_actor({{1251, -500}, 50, 100, 0, 1000, true, true, false}));
            removed = value(host->add_actor({{1200, -500}, 50, 100, 0, 1000, true, true, false}));
            ineligible = value(host->add_actor({{1200, -500}, 50, 100, 0, 1000, true, true, false}));
            auto ticket = value(host->prepare(after->accepted()->plan(), {caster, {}, Point{1000, -500}}));
            old_ticket = ticket;
            value(host->release(ticket));
            auto change = [&](ActorId id, Point p, bool eligible = true) {
                auto data = host->snapshot().actors[id.slot].data;
                data.position = p;
                data.eligible = eligible;
                done(host->update_actor(id, data));
            };
            change(entered, {1101, -500});
            change(left_inner, {1100, -500});
            change(entered_outer, {1150, -300});
            done(host->remove_actor(removed));
            change(ineligible, {1200, -500}, false);
            value(host->advance_to(99));
            saved = value(host->checkpoint());
            // Subsequent book edits and input destruction cannot change the captured pending shape or aim.
            auto edited = value(book->edit("burst", read("S3.verse"), "Later ordinary disk revision.", schema, learner));
            require(!edited->problem(), "later edit accepts");
        }
        auto host = value(ReferenceHost::create(schema));
        done(host->restore(saved));
        auto stale = host->release(old_ticket);
        require(!stale && stale.error().code == Reference::Error::ForeignTicket, "fresh checkpoint incarnation");
        require(host->snapshot().tick == 99 && host->snapshot().pending[0].aim == Point{1000, -500}, "retained point/clock");
        value(host->advance_to(100));
        const auto state = host->snapshot();
        require(
            state.actors[protected_centre.slot].data.health == 50 && state.actors[protected_centre.slot].slow == Slow{},
            "protected centre unchanged"
        );
        require(
            state.actors[entered.slot].data.health == 42 && state.actors[entered.slot].slow == Slow{750, 300},
            "current inner entrant hit by both effects"
        );
        require(
            state.actors[left_inner.slot].data.health == 50 && state.actors[left_inner.slot].slow == Slow{},
            "inner boundary protects departed ring member"
        );
        require(
            state.actors[entered_outer.slot].data.health == 42 && state.actors[entered_outer.slot].slow == Slow{750, 300},
            "outer boundary entrant hit"
        );
        require(
            !state.actors[removed.slot].present && state.actors[ineligible.slot].data.health == 50, "current removal/eligibility honored"
        );
        require(state.actors[0].data.mana == 80 && !state.pending[0].present, "paid once, pending consumed");
        value(host->advance_to(300));
        require(host->snapshot().actors[entered.slot].slow == Slow{}, "expiry200 ticks after activation");
        require(
            disk->plan()->meaning().bindings[0].query == Query::EligibleDisk && disk->plan()->meaning().bindings[0].inner_radius_mm == 0,
            "old disk plan remains disk"
        );
        std::cout
            << "Restored tick100: centre50, moved into ring42/slow750, moved to inner boundary50, entered outer boundary42/slow750. Tick300 expires. Mana80.\n";
        std::cout << "1 annular lesson-edit-move-checkpoint-cast caller passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
