#include <magic/learning.h>
#include <magic/reference-host.h>

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
    using namespace Magic;
    using namespace Magic::Learning;
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
        require(in.good(), "opens authored fixture");
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
        auto learner = value(Learner::create(all_rules & ~rule_bit(Rule::CinderSlow) & ~rule_bit(Rule::MendSlow)));
        auto book = value(Spellbook::create());
        done(book->add("restoration", "A restoration and a borrowed cadence"));
        auto initial = value(book->edit("restoration", read("S1.verse"), "Invented Mend healing.", schema, learner));
        require(!initial->problem(), "existing heal accepted");
        auto host = value(ReferenceHost::create(schema));
        auto caster = value(host->add_actor({{275, 0}, 100, 100, 100, 1000, false, true, true}));
        auto target = value(host->add_actor({{300, 0}, 50, 100, 0, 1000, true, true, false}));
        value(host->preview(initial->accepted()->plan(), {caster, target, {}}));
        auto composed = read("m4/composed.verse");
        auto unknown = value(book->edit("restoration", composed, "Transfer the duration-first production.", schema, learner));
        require(
            unknown->problem() && unknown->problem()->code == Learning::Error::UnknownKnowledge &&
                unknown->problem()->missing == rule_bit(Rule::CinderSlow),
            "recognized but unknown exact rule"
        );
        std::cout << "recognized, not learned: " << rule_key(Rule::CinderSlow) << " (invented Cinder: duration, rate, neth)\n";
        LessonHandle lesson;
        {
            auto original = read("m4/worked.verse"), gloss = read("m4/worked.common");
            auto practice = read("m4/practice.verse"), practice_gloss = read("m4/practice.common");
            lesson = value(make_lesson(
                {"borrowed-cadence", "cinder.slow@1",
                 "Invented Cinder/Common. Keep duration inside its target bridge; replace fire with Mend healing.", original, gloss,
                 practice, practice_gloss},
                schema
            ));
        }
        for (const auto& alignment : lesson->aligned())
            std::cout << alignment.original << "\n  = " << alignment.gloss << '\n';
        require(value(learner.learn(lesson)), "one production learned");
        require(!value(learner.learn(lesson)), "lesson idempotent");
        lesson.reset();
        auto accepted =
            value(book->edit("restoration", composed, "Invented Mend + Cinder, transferred to radius300mm and healing9.", schema, learner));
        require(
            !accepted->problem() && accepted->accepted()->revision() == 2 && learner.missing(*accepted->accepted()->plan()) == 0,
            "new revision available"
        );
        require(accepted->accepted()->common() == read("m4/composed.common"), "independent exact new Common");
        std::cout << accepted->accepted()->common() << '\n';
        auto disabled = schema;
        disabled.operations[2].max_quantity = 599;
        done(host->replace_catalog(disabled));
        auto no_permission = host->prepare(accepted->accepted()->plan(), {caster, {}, Point{0, 0}});
        require(
            !no_permission && no_permission.error().code == Reference::Error::SchemaMismatch, "learned does not grant disabled host effect"
        );
        std::cout << "learned, host refuses current schema: SchemaMismatch\n";
        done(host->replace_catalog(schema));
        auto broken = value(book->edit("restoration", composed + " then", "Repair this draft.", schema, learner));
        require(broken->problem() && broken->accepted() == accepted->accepted(), "failed edit preserves accepted revision2");
        std::cout << "failed draft: " << broken->problem()->diagnostic->message << " [byte " << broken->problem()->diagnostic->span.offset
                  << "]\n";
        auto ticket = value(host->prepare(book->accepted("restoration")->plan(), {caster, {}, Point{0, 0}}));
        value(host->release(ticket));
        value(host->advance_to(49));
        require(host->snapshot().actors[target.slot].data.health == 50, "before activation");
        value(host->advance_to(50));
        require(
            host->snapshot().actors[target.slot].data.health == 59 && host->snapshot().actors[target.slot].slow == Slow{600, 350},
            "new shared-payload effects"
        );
        value(host->advance_to(350));
        require(host->snapshot().actors[target.slot].slow == Slow{}, "exact expiry");
        require(
            initial->accepted()->revision() == 1 && initial->accepted()->plan()->meaning().effects[0].quantity == 12,
            "old healing handle immutable"
        );
        std::cout << "cast revision2: tick49 hp50; tick50 hp59 speed600; tick350 speed1000. Caster mana80.\n";
        require(host->snapshot().actors[caster.slot].data.mana == 80, "actual resource debit");
        std::cout << "1 teach-compose-cast caller passed (recognition, lesson, host refusal, failed edit, real cast).\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
