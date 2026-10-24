#include <magic/learning.h>
#include <magic/reference-host.h>
#include <iostream>
#include <stdexcept>

namespace {
    using namespace Magic;
    namespace R = Magic::Reference;
    namespace L = Magic::Learning;
    void check(bool value) {
        if (!value)
            throw std::runtime_error("caller expectation failed");
    }
    template <class T, class E> T take(std::expected<T, E> value) {
        if (!value)
            throw std::runtime_error("caller refusal");
        return std::move(*value);
    }
    template <class E> void ok(std::expected<void, E> value) {
        check(bool(value));
    }
}
int main() {
    try {
        Catalog game;
        game.operations = {{Primitive::Ward, "game.ward"}, {Primitive::Restrict, "game.restrict"}, {Primitive::Fire, "game.fire"}};
        auto host = take(R::ReferenceHost::create(game));
        auto caster = take(host->add_actor({{0, 0}, 100, 100, 100, 1000, false, true, true}));
        auto ally = take(host->add_actor({{10, 0}, 50, 100, 0, 1000, true, true, false}));
        auto attacker = take(host->add_actor({{-10, 0}, 100, 100, 0, 1000, true, true, false}));
        auto learned = take(L::Learner::create(all_rules));
        auto book = take(L::Spellbook::create());
        ok(book->add("ward", "Guarded passage"));
        const std::string source =
            "spoken release via contact after 0 ms { bind hand = willing touched actor at release ; together ordered { on hand bridge Common@1 { ward absorb 5 dp for 2000 ms } ; } }";
        auto first = take(book->edit("ward", source, "Protect against real attacks.", game, learned));
        check(!first->problem());
        auto accepted = first->accepted();
        auto failed = take(book->edit("ward", "broken verse", "Repair this draft.", game, learned));
        check(failed->problem() && failed->accepted() == accepted && accepted->revision() == 1);
        auto changed = source;
        changed.replace(changed.find("2000"), 4, "3000");
        auto second = take(book->edit("ward", changed, "Longer guard.", game, learned));
        check(second->accepted()->revision() == 2 && second->accepted()->plan() != accepted->plan());
        auto invalid = host->release({});
        check(!invalid && invalid.error().code == R::Error::InvalidTicket);
        take(host->release(take(host->prepare(accepted->plan(), {caster, ally, {}}))));
        auto hit = take(host->try_attack(attacker, ally));
        check(hit->damage == R::DamageResolution{3, 3, 0});
        auto fire = take(compile(
            "spoken release via contact after 0 ms { bind hand = willing touched actor at release ; together ordered { on hand bridge Common@1 { fire damage 4 dp instant } ; } }",
            game
        ));
        take(host->release(take(host->prepare(fire, {caster, ally, {}}))));
        check(host->snapshot().actors[ally.slot].data.health == 48 && take(host->inspect_mechanics()).wards[0].instance == 0);
        auto seal = take(compile(
            "spoken release via contact after 0 ms { bind hand = willing touched actor at release ; together ordered { on hand bridge Common@1 { forbid attack for 1000 ms } ; } }",
            game
        ));
        take(host->release(take(host->prepare(seal, {caster, attacker, {}}))));
        auto refused = host->try_attack(attacker, ally);
        check(!refused && refused.error().code == R::Error::RestrictedIntent);
        auto cp = take(host->checkpoint());
        host.reset();
        host = take(R::ReferenceHost::create(game));
        ok(host->restore(cp));
        take(host->advance_to(100));
        auto resumed = take(host->try_attack(attacker, ally));
        check(
            resumed->damage == R::DamageResolution{3, 0, 3} && host->snapshot().actors[ally.slot].data.health == 45 &&
            host->snapshot().actors[caster.slot].data.mana == 70
        );
        check(hit->damage == R::DamageResolution{3, 3, 0});
        std::cout
            << accepted->common()
            << "\nWard5: hit3 prevents3, fire4 prevents2/passes2; attack seal refuses until100. Restored attack hp48->45, mana70; failed draft and old revision remain owned.\n1 M7 mechanics caller passed.\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
