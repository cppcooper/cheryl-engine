#include <magic/reference-host.h>
#include <iostream>
#include <stdexcept>

namespace {
    using namespace Magic;
    namespace R = Magic::Reference;
    void check(bool v) {
        if (!v)
            throw std::runtime_error("material caller expectation");
    }
    template <class T, class E> T take(std::expected<T, E> v) {
        if (!v)
            throw std::runtime_error("material caller refusal");
        return std::move(*v);
    }
    template <class E> void ok(std::expected<void, E> v) {
        check(bool(v));
    }
    Catalog catalog() {
        Catalog c;
        c.operations = {
            {Primitive::Create, "game.create"},
            {Primitive::Transmute, "game.transmute"},
            {Primitive::Relocate, "game.relocate"},
            {Primitive::Disguise, "game.disguise"}
        };
        return c;
    }
}
int main() {
    try {
        R::Checkpoint checkpoint;
        R::ActorId caster, owner, other;
        R::ObjectId wall;
        R::Perceived old_view;
        R::Ticket old_ticket;
        {
            auto game = catalog();
            auto h = take(R::ReferenceHost::create(game));
            caster = take(h->add_actor({{0, 0}, 100, 100, 100, 1000, false, true, true}));
            owner = take(h->add_actor({{10, 0}, 50, 100, 0, 1000, true, true, false}));
            other = take(h->add_actor({{-10, 0}, 100, 100, 0, 1000, false, true, false}));
            const std::string create_source =
                "spoken release via contact after 0 ms { bind owner = willing touched actor at release ; together ordered { on owner bridge Common@1 { create stone-wall at G0 point 300 mm 0 mm for 4000 ms owned by this actor } ; } }";
            auto create = take(compile(create_source, game));
            old_ticket = take(h->prepare(create, {caster, owner, {}}));
            take(h->release(old_ticket));
            wall = {0, 1};
            auto facts = take(h->inspect_perception());
            check(facts.objects[0].data == R::ObjectSpec{Appearance::StoneWall, false, {300, 0}});
            check(take(h->inspect_mechanics()).creations[0].owner == owner);
            auto disguise = take(compile(
                "spoken release via burst after 0 ms { bind audience = eligible actors in G0 disk 250 mm around aim captured at release queried at activation inclusive ; together ordered { on audience bridge Common@1 { show subject captured at release rechecked at activation as stone-wall for 2000 ms to this audience frozen at activation } ; } }",
                game
            ));
            take(h->release(take(h->prepare(disguise, {caster, {}, R::Point{}, wall}))));
            auto transmute = take(compile(
                "spoken release via subject after 0 ms { bind object = object captured at release rechecked at activation ; together ordered { on object bridge Common@1 { transmute object to open-arch for 1000 ms } ; } }",
                game
            ));
            take(h->release(take(h->prepare(transmute, {caster, {}, {}, wall}))));
            old_view = take(h->perceive(owner, wall));
            check(
                old_view.form == Appearance::StoneWall &&
                take(h->inspect_perception()).objects[0].data == R::ObjectSpec{Appearance::OpenArch, true, {300, 0}}
            );
            auto relocate = take(compile(
                "spoken release via contact after 0 ms { bind mover = willing touched actor at release ; together ordered { on mover bridge Common@1 { relocate to G0 point 300 mm 0 mm instant } ; } }",
                game
            ));
            take(h->release(take(h->prepare(relocate, {caster, owner, {}}))));
            check(
                h->snapshot().actors[owner.slot].data.position == R::Point{300, 0} && h->snapshot().actors[owner.slot].data.health == 50 &&
                h->snapshot().actors[caster.slot].data.mana == 60
            );
            std::cout << create->common() << '\n' << transmute->common() << '\n';
            checkpoint = take(h->checkpoint());
        } // All source/catalog/old host dies; checkpoint owns actual plans and contribution state.
        {
            auto game = catalog();
            auto h = take(R::ReferenceHost::create(game));
            ok(h->restore(checkpoint));
            auto stale = h->release(old_ticket);
            check(!stale && stale.error().code == R::Error::ForeignTicket);
            take(h->advance_to(99));
            check(take(h->inspect_perception()).objects[0].data.passable);
            take(h->advance_to(100));
            check(
                !take(h->inspect_perception()).objects[0].data.passable &&
                h->snapshot().actors[owner.slot].data.position == R::Point{300, 0}
            );
            auto refused = h->dismiss_created(other, wall);
            check(!refused && refused.error().code == R::Error::Permission);
            ok(h->dismiss_created(owner, wall));
            auto replacement = take(compile(
                "spoken release via contact after 0 ms { bind owner = willing touched actor at release ; together ordered { on owner bridge Common@1 { create open-arch at G0 point 600 mm 0 mm for 4000 ms owned by this actor } ; } }",
                game
            ));
            auto other_data = h->snapshot().actors[other.slot].data;
            other_data.eligible = true;
            ok(h->update_actor(other, other_data));
            take(h->release(take(h->prepare(replacement, {caster, other, {}}))));
            check(take(h->inspect_perception()).objects[0].id == R::ObjectId{0, 2});
            take(h->advance_to(400));
            check(take(h->inspect_perception()).objects[0].present);
            take(h->advance_to(500));
            check(
                !take(h->inspect_perception()).objects[0].present && h->snapshot().actors[owner.slot].data.health == 50 &&
                h->snapshot().actors[caster.slot].data.mana == 50
            );
        }
        check(old_view == R::Perceived{owner, wall, Appearance::StoneWall, 0});
        std::cout
            << "1 M7 material caller passed: created wall0:1, physical arch/perceived wall, relocation to300, restore/expiry100, owner-only dismissal, new0:2 survives old400 and expires500; hp50/mana50. Point occupancy is not collision physics.\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
