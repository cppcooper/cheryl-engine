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
            throw std::runtime_error("caller literal expectation failed");
    }
    template <class T, class E> T take(std::expected<T, E> v) {
        if (!v)
            throw std::runtime_error("caller refusal");
        return std::move(*v);
    }
    template <class E> void ok(std::expected<void, E> v) {
        check(bool(v));
    }
}
int main() {
    try {
        R::Checkpoint checkpoint;
        R::ActorId caster, target, other, servant;
        R::ObjectId arch;
        R::ItemId rune;
        R::InfluenceRef old_restriction, old_disguise;
        R::ServantCommand old_command;
        R::TouchEvent old_event;
        R::Ticket old_ticket;
        R::Perceived old_view;
        PlanHandle dispel_restriction, dispel_disguise;
        R::Observed old_release;
        Catalog compatible;
        {
            Catalog catalog;
            catalog.operations = {{Primitive::Animate, "fixture.animate"},   {Primitive::Dispel, "fixture.dispel"},
                                  {Primitive::Restrict, "fixture.restrict"}, {Primitive::Disguise, "fixture.disguise"},
                                  {Primitive::Discern, "fixture.discern"},   {Primitive::Heal, "fixture.heal"},
                                  {Primitive::Fire, "fixture.fire"},         {Primitive::Slow, "fixture.slow"}};
            compatible = catalog; // destination's exact current schema; no plan regeneration
            auto host = take(R::ReferenceHost::create(catalog));
            caster = take(host->add_actor({{0, 0}, 100, 100, 100, 1000, false, true, true}));
            target = take(host->add_actor({{20, 0}, 50, 100, 0, 1000, true, true, false}));
            other = take(host->add_actor({{30, 0}, 60, 100, 0, 1000, true, true, false}));
            auto remains = take(host->add_remains({10, 0}));
            auto learner = take(L::Learner::create(all_rules));
            auto book = take(L::Spellbook::create());
            ok(book->add("servant", "A borrowed moment"));
            std::string verse =
                "spoken release via remains after 0 ms { bind body = remains captured at release ; together ordered { on body bridge Common@1 { animate bound remains for 2000 ms owned by caster } ; } }";
            auto accepted =
                take(book->edit("servant", verse, "Registered remains, no soul or inventory transfer.", catalog, learner))->accepted();
            check(accepted && accepted->revision() == 1);
            verse.replace(verse.find("2000"), 4, "3000");
            auto revised = take(book->edit("servant", verse, "Longer future revision.", catalog, learner))->accepted();
            auto failed = take(book->edit("servant", "animate arbitrary corpse", "Repair the explicit role.", catalog, learner));
            check(revised->revision() == 2 && failed->problem() && failed->accepted() == revised);
            check(!host->release({})); // knowledge/accepted revision is not authority
            old_release = take(host->release(take(host->prepare(accepted->plan(), {caster, {}, {}, {}, remains}))));
            servant = take(host->life()).servants[0].actor;
            check(servant == R::ActorId{3, 1} && take(host->life()).remains[0].spent && take(host->life()).servants[0].until_tick == 200);
            auto restriction = take(compile(
                "spoken release via contact after 0 ms { bind hand = willing touched actor at release ; together ordered { on hand bridge Common@1 { forbid attack for 3000 ms } ; } }",
                catalog
            ));
            take(host->release(take(host->prepare(restriction, {caster, servant, {}}))));
            old_restriction = take(host->bind_restriction(1, servant));
            old_command = take(host->command_authority(caster, servant));
            auto refused = host->command_attack(old_command, target);
            check(!refused && refused.error().code == R::Error::RestrictedIntent && host->snapshot().actors[target.slot].data.health == 50);
            arch = take(host->add_object({Appearance::OpenArch, true, {300, 0}}));
            auto disguise = take(compile(
                "spoken release via burst after 0 ms { bind audience = eligible actors in G0 disk 250 mm around aim captured at release queried at activation inclusive ; together ordered { on audience bridge Common@1 { show subject captured at release rechecked at activation as stone-wall for 4000 ms to this audience frozen at activation } ; } }",
                catalog
            ));
            take(host->release(take(host->prepare(disguise, {caster, {}, R::Point{0, 0}, arch}))));
            old_disguise = take(host->bind_disguise(1, arch));
            old_view = take(host->perceive(other, arch));
            auto discern = take(compile(
                "spoken release via burst after 0 ms { bind audience = eligible actors in G0 disk 1 mm around aim captured at release queried at activation inclusive ; together ordered { on audience bridge Common@1 { discern subject captured at release rechecked at activation for this audience instant } ; } }",
                catalog
            ));
            take(host->release(take(host->prepare(discern, {caster, {}, R::Point{20, 0}, arch}))));
            check(
                take(host->perceive(target, arch)).form == Appearance::OpenArch &&
                take(host->perceive(other, arch)).form == Appearance::StoneWall
            );
            check(take(host->knowledge(target)).count == 1 && take(host->knowledge(other)).count == 0);
            auto engraving = take(compile(
                "engraved item-touch charges 1 release via item-event after 0 ms { bind toucher = willing event toucher at activation ; together ordered { on toucher bridge Common@1 { heal 12 hp instant } ; } }",
                catalog
            ));
            rune = take(host->install(engraving, caster));
            old_event = take(host->admit_touch(1, rune, target));
            auto delayed = take(compile(
                "spoken release via burst after 1000 ms { bind ring = eligible actors in G0 annulus excluding inner disk 100 mm inclusive within outer disk 250 mm inclusive around aim captured at release queried at activation ; together ordered { on ring bridge Common@1 { fire damage 4 dp instant } ; on ring bridge Common@1 { multiply speed by 750 permille for 2000 ms } ; } }",
                catalog
            ));
            // Only other enters this captured annulus; the servant/target remain in its protected centre.
            auto moved = host->snapshot().actors[other.slot].data;
            moved.position = {150, 0};
            ok(host->update_actor(other, moved));
            take(host->release(take(host->prepare(delayed, {caster, {}, R::Point{0, 0}}))));
            dispel_restriction = take(compile(
                "spoken release via influence after 0 ms { bind seal = influence captured at release ; together ordered { on seal bridge Common@1 { dispel attack-restriction bound influence instant } ; } }",
                catalog
            ));
            dispel_disguise = take(compile(
                "spoken release via influence after 0 ms { bind veil = influence captured at release ; together ordered { on veil bridge Common@1 { dispel disguise bound influence instant } ; } }",
                catalog
            ));
            old_ticket = take(host->prepare(dispel_restriction, {caster, {}, {}, {}, {}, old_restriction}));
            checkpoint = take(host->checkpoint());
            check(
                host->snapshot().actors[caster.slot].data.mana == 30 && host->snapshot().pending[0].due_tick == 100 &&
                host->items().items[rune.slot].charges == 1
            );
            std::cout
                << accepted->common()
                << "\nBefore restore: remains0:1 spent, servant3:1 hp20 owned by0:1 until200; restriction blocks attack; mana30, paid annular cast due100.\n";
        } // source, book, learner, input catalog and original host are destroyed
        auto restored = take(R::ReferenceHost::create(compatible));
        ok(restored->restore(checkpoint));
        check(!restored->release(old_ticket) && !restored->command_attack(old_command, target) && !restored->touch(old_event));
        auto foreign = restored->prepare(dispel_restriction, {caster, {}, {}, {}, {}, old_restriction});
        check(!foreign && foreign.error().code == R::Error::ForeignCapability);
        auto restriction = take(restored->bind_restriction(1, servant));
        auto disabled = restored->snapshot().actors[caster.slot].data;
        disabled.can_cast = false;
        ok(restored->update_actor(caster, disabled));
        auto forbidden = restored->prepare(dispel_restriction, {caster, {}, {}, {}, {}, restriction});
        check(!forbidden && forbidden.error().code == R::Error::Permission);
        disabled.can_cast = true;
        ok(restored->update_actor(caster, disabled));
        take(restored->release(take(restored->prepare(dispel_restriction, {caster, {}, {}, {}, {}, restriction}))));
        auto command = take(restored->command_authority(caster, servant));
        auto attack = take(restored->command_attack(command, target));
        check(attack->damage == R::DamageResolution{3, 0, 3} && restored->snapshot().actors[target.slot].data.health == 47);
        auto disguise = take(restored->bind_disguise(1, arch));
        take(restored->release(take(restored->prepare(dispel_disguise, {caster, {}, {}, {}, {}, disguise}))));
        check(
            take(restored->perceive(other, arch)).form == Appearance::OpenArch &&
            take(restored->inspect_perception()).objects[arch.slot].data.passable && take(restored->knowledge(target)).count == 1 &&
            take(restored->knowledge(other)).count == 0
        );
        take(restored->touch(take(restored->resume_touch(1))));
        check(restored->snapshot().actors[target.slot].data.health == 59 && restored->items().items[rune.slot].charges == 0);
        take(restored->advance_to(99));
        check(restored->snapshot().actors[other.slot].data.health == 60);
        take(restored->advance_to(100));
        check(
            restored->snapshot().actors[other.slot].data.health == 56 &&
            restored->snapshot().actors[other.slot].slow == R::Slow{750, 300} && restored->snapshot().actors[target.slot].data.health == 59
        );
        take(restored->advance_to(199));
        check(restored->snapshot().actors[servant.slot].present);
        take(restored->advance_to(200));
        check(!restored->snapshot().actors[servant.slot].present && take(restored->life()).remains[0].spent);
        auto replacement = take(restored->add_actor({{10, 0}, 70}));
        check(replacement == R::ActorId{3, 2});
        take(restored->advance_to(300));
        check(
            restored->snapshot().actors[other.slot].slow == R::Slow{} && restored->snapshot().actors[replacement.slot].present &&
            restored->snapshot().actors[caster.slot].data.mana == 10
        );
        check(old_view.form == Appearance::StoneWall && old_view.tick == 0 && old_release->after.actors[servant.slot].data.health == 20);
        std::cout
            << "Restored: exact dispel pays10, servant attack hp50->47; disguise removal changes B wall->arch with passability true and A-only dated knowledge retained. Rune heals47->59 once. Annulus at100 hits B60->56/slow until300; servant expires200, replacement3:2 survives; mana10.\n1 M8 lifecycle caller passed.\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
