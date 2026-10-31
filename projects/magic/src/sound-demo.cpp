#include <magic/reference-host.h>
#include <iostream>
#include <stdexcept>

namespace {
    using namespace Magic;
    namespace R = Magic::Reference;
    void check(bool valid) {
        if (!valid)
            throw std::runtime_error("caller literal mismatch");
    }
    template <class T, class E> T take(std::expected<T, E> v) {
        if (!v)
            throw std::runtime_error("caller refusal");
        return std::move(*v);
    }
}
int main() {
    try {
        Catalog catalog;
        catalog.operations = {{Primitive::Heal, "heal"}, {Primitive::Sound, "sound"}};
        auto spoken = take(compile(
            "spoken release via contact after 0 ms { bind listener = willing touched actor at release ; together ordered { on listener bridge Common@1 { emit footsteps at G0 offset -2000 mm 0 mm from this listener captured at activation for 1000 ms audible only to this listener } ; } }",
            catalog
        ));
        auto host = take(R::ReferenceHost::create(catalog));
        auto caster = take(host->add_actor({{2100, 30}, 100, 100, 100, 1000, false, true, true}));
        auto a = take(host->add_actor({{2100, 30}, 50}));
        auto b = take(host->add_actor({{2110, 30}, 60}));
        auto wall = take(host->add_object({Appearance::StoneWall, false, {500, 30}}));
        auto physical = take(host->inspect_perception());
        auto a_ref = take(host->bind_listener(a)), b_ref = take(host->bind_listener(b));
        take(host->release(take(host->prepare(spoken, {caster, a, {}}))));
        auto first = take(host->hear(a_ref));
        check(
            first.listener == a && first.tick == 0 && first.count == 1 &&
            first.sounds[0] == R::HeardSound{SoundKind::Footsteps, {100, 30}} && take(host->hear(b_ref)).count == 0 &&
            host->snapshot().actors[caster.slot].data.mana == 90
        );
        take(host->advance_to(100));
        check(take(host->hear(a_ref)).count == 0);

        R::Checkpoint checkpoint;
        R::TouchEvent old_event;
        R::ItemId rune;
        {
            auto verse = take(compile(
                "engraved item-touch charges 1 release via item-event after 0 ms { bind listener = willing event toucher at activation ; together ordered { on listener bridge Common@1 { heal 3 hp instant } ; on listener bridge Common@1 { emit footsteps at G0 offset -2000 mm 0 mm from this listener captured at activation for 2000 ms audible only to this listener } ; } }",
                catalog
            ));
            rune = take(host->install(verse, caster));
            old_event = take(host->admit_touch(17, rune, a));
            auto actor = host->snapshot().actors[a.slot].data;
            actor.position = {2300, 40};
            check(bool(host->update_actor(a, actor)));
            take(host->touch(old_event));
            checkpoint = take(host->checkpoint());
        }
        auto old_hearing = take(host->hear(a_ref));
        check(old_hearing.tick == 100 && old_hearing.count == 1 && old_hearing.sounds[0] == R::HeardSound{SoundKind::Footsteps, {300, 40}});
        host.reset();
        host = take(R::ReferenceHost::create(catalog));
        check(bool(host->restore(checkpoint)));
        auto stale = host->hear(a_ref);
        check(!stale && stale.error().code == R::Error::ForeignCapability);
        auto repeated = host->touch(old_event);
        check(!repeated && repeated.error().code == R::Error::ForeignEvent);
        a_ref = take(host->bind_listener(a));
        b_ref = take(host->bind_listener(b));
        auto current = take(host->hear(a_ref));
        check(
            current == old_hearing && take(host->hear(b_ref)).count == 0 && host->snapshot().actors[a.slot].data.health == 53 &&
            host->snapshot().actors[caster.slot].data.mana == 70 && host->items().items[rune.slot].charges == 0 &&
            host->items().ledger[0].consumed && take(host->knowledge(a)).count == 0 && take(host->knowledge(b)).count == 0 &&
            take(host->inspect_perception()) == physical && take(host->inspect_perception()).objects[wall.slot].data.passable == false
        );
        auto actor = host->snapshot().actors[a.slot].data;
        actor.position = {9000, 50};
        check(bool(host->update_actor(a, actor)));
        take(host->advance_to(299));
        check(take(host->hear(a_ref)).sounds[0].point == R::Point{300, 40});
        take(host->advance_to(300));
        check(take(host->hear(a_ref)).count == 0 && old_hearing.sounds[0].point == R::Point{300, 40} && first.tick == 0);
        std::cout
            << spoken->common()
            << "\nSpoken: A hears footsteps at(100,30), B none, expiry100, mana90. Accepted rune touch at100 captures A(2300,40)+offset(-2000,0)=(300,40), hp53/mana70/charge0. Restore renews listener/event authority; point stays fixed after movement and expires300. Wall/truth/knowledge unchanged.\n1 M10 sound caller passed.\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
