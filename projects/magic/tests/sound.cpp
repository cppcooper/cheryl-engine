#include <magic/reference-host.h>
#include <magic/learning.h>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <type_traits>

namespace {
    using namespace Magic;
    namespace R = Magic::Reference;
    namespace L = Magic::Learning;
    int passed = 0, failed = 0;
    void check(bool valid) {
        if (!valid)
            throw std::runtime_error("literal mismatch");
    }
    template <class T, class E> T take(std::expected<T, E> v) {
        if (!v) {
            if constexpr (requires { R::error_name(v.error().code); })
                throw std::runtime_error(std::string(R::error_name(v.error().code)));
            else
                throw std::runtime_error("unexpected refusal");
        }
        return std::move(*v);
    }
    template <class E> void ok(std::expected<void, E> v) {
        check(bool(v));
    }
    template <class T, class E, class C> void refuses(std::expected<T, E> v, C c) {
        check(!v && v.error().code == c);
    }
    void test(std::string_view name, std::function<void()> run) {
        try {
            run();
            ++passed;
            std::cout << "PASS " << name << '\n';
        } catch (const std::exception& e) {
            ++failed;
            std::cout << "FAIL " << name << ": " << e.what() << '\n';
        }
    }
    Catalog game() {
        Catalog c;
        c.operations = {
            {Primitive::Heal, "heal"},
            {Primitive::Fire, "fire"},
            {Primitive::Slow, "slow"},
            {Primitive::Sound, "sound"},
            {Primitive::Dispel, "dispel"}
        };
        return c;
    }
    std::string sound(std::string_view offset = "-2000 mm 0 mm", std::string_view duration = "2000 ms") {
        return "emit footsteps at G0 offset " + std::string(offset) + " from this listener captured at activation for " +
               std::string(duration) + " audible only to this listener";
    }
    std::string verse(std::string_view clause, bool engraved = false) {
        return std::string(engraved ? "engraved item-touch charges 1 release via item-event" : "spoken release via contact") +
               " after 0 ms { bind listener = " + (engraved ? "willing event toucher at activation" : "willing touched actor at release") +
               " ; together ordered { on listener bridge Common@1 { " + std::string(clause) + " } ; } }";
    }
    std::string composed(bool engraved = false) {
        auto s = verse("heal 3 hp instant", engraved);
        s.insert(s.rfind("} }"), "on listener bridge Common@1 { " + sound() + " } ; ");
        return s;
    }
    std::string burst(std::string_view clause, std::uint32_t delay = 1000) {
        return "spoken release via burst after " + std::to_string(delay) +
               " ms { bind near = eligible actors in G0 disk 100 mm around aim captured at release queried at activation inclusive ; together ordered { on near bridge Common@1 { " +
               std::string(clause) + " } ; } }";
    }
    std::string replace(std::string s, std::string_view a, std::string_view b) {
        auto at = s.find(a);
        check(at != s.npos);
        s.replace(at, a.size(), b);
        return s;
    }
    PlanHandle plan(std::string source) {
        return take(compile(source, game()));
    }
    struct Fixture {
        std::unique_ptr<R::ReferenceHost> host;
        R::ActorId caster, a, b;
        Fixture(R::HostLimits limits = {}) {
            host = take(R::ReferenceHost::create(game(), limits));
            caster = take(host->add_actor({{0, 0}, 100, 100, 1000, 1000, false, true, true}));
            a = take(host->add_actor({{20, 0}, 50}));
            b = take(host->add_actor({{40, 0}, 60, 100, 0, 1000, false, true, false}));
        }
        R::Observed cast(PlanHandle p, R::Request r) { return take(host->release(take(host->prepare(p, r)))); }
        R::Observed emit(std::string_view offset = "-2000 mm 0 mm", std::string_view duration = "2000 ms") {
            return cast(plan(verse(sound(offset, duration))), {caster, a, {}});
        }
        R::ListenerRef listener(R::ActorId id) { return take(host->bind_listener(id)); }
        R::Hearing hear(R::ActorId id) { return take(host->hear(listener(id))); }
        template <class F> void unchanged(F run) {
            auto w = host->snapshot();
            auto s = take(host->sounds());
            auto i = host->items();
            auto p = take(host->inspect_perception());
            auto slow = take(host->slows());
            run();
            check(
                w == host->snapshot() && s == take(host->sounds()) && i == host->items() && p == take(host->inspect_perception()) &&
                slow == take(host->slows())
            );
        }
    };
}
int main() {
    static_assert(!std::is_convertible_v<R::ListenerRef, R::Ticket> && !std::is_convertible_v<R::SoundSnapshot, R::Checkpoint>);
    test("typed sound exact Common and concept", [] {
        auto p = plan(verse(sound()));
        check(p->meaning().effects[0] == Effect{Primitive::Sound, "sound", 0, 0, 0, DecoySound{SoundKind::Footsteps, {-2000, 0}, 200}});
        check(
            p->requirements() == (rule_bit(Rule::CommonSound) | rule_bit(Rule::TargetBridge)) &&
            rule_key(Rule::CommonSound) == "common.sound@1"
        );
        check(take(compile(p->common(), game()))->meaning() == p->meaning());
    });
    test("registered kind dimensions duration and frame refuse", [] {
        refuses(compile(verse(replace(sound(), "footsteps", "voice")), game()), Code::UnknownPhrase);
        for (auto duration : {"0 ms", "1 ms", "-10 ms", "99999999999999999999999999999 ms", "2 hp"})
            check(!compile(verse(sound("0 mm 0 mm", duration)), game()));
        for (auto offset : {"1 hp 0 mm", "1000001 mm 0 mm", "0 mm 99999999999999999999 mm"})
            check(!compile(verse(sound(offset)), game()));
        check(!compile(verse(replace(sound(), "G0", "G1")), game()));
    });
    test("sound capability bounds and legacy schema aggregates", [] {
        auto c = game();
        c.operations[3].sound_kinds = 0;
        refuses(compile(verse(sound()), c), Code::UnsupportedOperation);
        c.operations[3].sound_kinds = 2;
        refuses(R::ReferenceHost::create(c), R::Error::InvalidCatalog);
        check(!compile(verse(sound()), c));
        c = game();
        c.operations[3].max_coordinate_mm = 1999;
        check(!compile(verse(sound()), c));
        OperationSchema legacy{Primitive::Sound, "sound", 1000000, 8640000, 3, 1000000};
        check(legacy.sound_kinds == 1);
        c = game();
        c.operations[3].max_coordinate_mm = 0;
        check(bool(compile(verse(sound("0 mm 0 mm")), c)));
    });
    test("scalar and sound composition keeps distinct domains", [] {
        for (bool reverse : {false, true}) {
            Fixture f;
            auto source = composed();
            source.insert(source.rfind("} }"), "on listener bridge Common@1 { multiply speed by 750 permille for 3000 ms } ; ");
            if (reverse) {
                source = replace(replace(source, "heal 3 hp instant", "SOUNDPLACEHOLDER"), sound(), "heal 3 hp instant");
                source = replace(source, "SOUNDPLACEHOLDER", sound());
            }
            f.cast(plan(source), {f.caster, f.a, {}});
            check(
                f.host->snapshot().actors[f.a.slot].data.health == 53 && f.host->snapshot().actors[f.a.slot].slow == R::Slow{750, 300} &&
                f.hear(f.a).sounds[0] == R::HeardSound{SoundKind::Footsteps, {-1980, 0}} && f.host->snapshot().actors[0].data.mana == 970
            );
            check(take(f.host->sounds()).influences[0].effect_index == (reverse ? 0u : 1u));
        }
    });
    test("duplicate sounds and unsupported domain envelopes refuse", [] {
        auto source = verse(sound());
        source.insert(source.rfind("} }"), "on listener bridge Common@1 { " + sound("0 mm 0 mm") + " } ; ");
        refuses(compile(source, game()), Code::UnsupportedComposition);
        check(!compile(replace(verse(sound()), "via contact", "via ray"), game()));
        check(!compile(replace(verse(sound(), true), "after 0 ms", "after 10 ms"), game()));
        check(!compile(
            replace(verse(sound()), "willing touched actor at release", "object captured at release rechecked at activation"), game()
        ));
    });
    test("two listeners isolate typed samples physical truth and knowledge", [] {
        Fixture f;
        auto wall = take(f.host->add_object({Appearance::StoneWall, false, {100, 0}}));
        auto physical = take(f.host->inspect_perception());
        auto a = f.listener(f.a), b = f.listener(f.b);
        f.emit();
        auto world = f.host->snapshot();
        auto sounds = take(f.host->sounds());
        auto heard = take(f.host->hear(a));
        check(
            heard.listener == f.a && heard.tick == 0 && heard.count == 1 &&
            heard.sounds[0] == R::HeardSound{SoundKind::Footsteps, {-1980, 0}}
        );
        check(
            take(f.host->hear(b)).listener == f.b && take(f.host->hear(b)).count == 0 && take(f.host->knowledge(f.a)).count == 0 &&
            take(f.host->knowledge(f.b)).count == 0 && take(f.host->inspect_perception()) == physical &&
            !physical.objects[wall.slot].data.passable
        );
        check(world == f.host->snapshot() && sounds == take(f.host->sounds()));
    });
    test("current default and foreign listener authority", [] {
        Fixture f, other;
        f.emit();
        other.emit("0 mm 0 mm");
        f.unchanged([&] { refuses(f.host->hear({}), R::Error::InvalidCapability); });
        auto foreign = other.listener(other.a);
        f.unchanged([&] { refuses(f.host->hear(foreign), R::Error::ForeignCapability); });
        auto actor = f.host->snapshot().actors[f.a.slot].data;
        actor.can_cast = false;
        ok(f.host->update_actor(f.a, actor));
        f.unchanged([&] { refuses(f.host->prepare(plan(verse(sound())), {f.a, f.a, {}}), R::Error::Permission); });
    });
    test("live caster permission funds and sound schema before staleness", [] {
        Fixture f;
        auto p = plan(verse(sound()));
        auto ticket = take(f.host->prepare(p, {f.caster, f.a, {}}));
        auto actor = f.host->snapshot().actors[0].data;
        actor.can_cast = false;
        ok(f.host->update_actor(f.caster, actor));
        f.unchanged([&] { refuses(f.host->release(ticket), R::Error::Permission); });
        actor.can_cast = true;
        actor.mana = 9;
        ok(f.host->update_actor(f.caster, actor));
        f.unchanged([&] { refuses(f.host->release(ticket), R::Error::Resources); });
        actor.mana = 10;
        ok(f.host->update_actor(f.caster, actor));
        auto catalog = game();
        catalog.operations[3].sound_kinds = 0;
        ok(f.host->replace_catalog(catalog));
        f.unchanged([&] { refuses(f.host->release(ticket), R::Error::SchemaMismatch); });
        ok(f.host->replace_catalog(game()));
        refuses(f.host->release(ticket), R::Error::StaleTicket);
        f.cast(p, {f.caster, f.a, {}});
        check(f.host->snapshot().actors[0].data.mana == 0);
    });
    test("live symbolic key primitive duration and coordinates", [] {
        for (unsigned field = 0; field < 4; ++field) {
            Fixture f;
            auto p = plan(verse(sound()));
            auto ticket = take(f.host->prepare(p, {f.caster, f.a, {}}));
            auto c = game();
            if (field == 0)
                c.operations[3].key = "new.sound";
            if (field == 1) {
                c.operations.erase(c.operations.begin() + 3);
                c.operations[1].key = "sound";
            }
            if (field == 2)
                c.operations[3].max_duration_ticks = 199;
            if (field == 3)
                c.operations[3].max_coordinate_mm = 1999;
            ok(f.host->replace_catalog(c));
            f.unchanged([&] { refuses(f.host->release(ticket), R::Error::SchemaMismatch); });
            ok(f.host->discard(ticket));
        }
    });
    test("point captures activation and remains frozen after movement", [] {
        Fixture f;
        auto p = plan(verse(sound()));
        auto ticket = take(f.host->prepare(p, {f.caster, f.a, {}}));
        auto actor = f.host->snapshot().actors[f.a.slot].data;
        actor.position = {30, 2};
        ok(f.host->update_actor(f.a, actor));
        refuses(f.host->release(ticket), R::Error::StaleTicket);
        f.cast(p, {f.caster, f.a, {}});
        actor.position = {9000, 3};
        ok(f.host->update_actor(f.a, actor));
        check(f.hear(f.a).sounds[0].point == R::Point{-1970, 2} && take(f.host->sounds()).influences[0].captured_anchor == R::Point{30, 2});
    });
    test("world point and resolved schema bounds refuse atomically", [] {
        Fixture f;
        auto actor = f.host->snapshot().actors[f.a.slot].data;
        actor.position = {1000000, 0};
        ok(f.host->update_actor(f.a, actor));
        auto caster = f.host->snapshot().actors[0].data;
        caster.position = actor.position;
        ok(f.host->update_actor(f.caster, caster));
        f.unchanged([&] { refuses(f.host->prepare(plan(verse(sound("1 mm 0 mm"))), {f.caster, f.a, {}}), R::Error::InvalidAim); });
        auto c = game();
        c.operations[3].max_coordinate_mm = 0;
        ok(f.host->replace_catalog(c));
        auto p = take(compile(verse(sound("0 mm 0 mm")), c));
        f.unchanged([&] { refuses(f.host->prepare(p, {f.caster, f.a, {}}), R::Error::SchemaMismatch); });
    });
    test("eight overlapping sounds capacity and chronological sampling after reuse", [] {
        Fixture f;
        f.emit("1 mm 0 mm", "1000 ms");
        for (unsigned i = 2; i <= 8; ++i)
            f.emit(std::to_string(i) + " mm 0 mm", "3000 ms");
        check(f.hear(f.a).count == 8);
        f.unchanged([&] { refuses(f.host->prepare(plan(verse(sound())), {f.caster, f.a, {}}), R::Error::SoundLimit); });
        take(f.host->advance_to(100));
        f.emit("99 mm 0 mm", "3000 ms");
        auto heard = f.hear(f.a);
        check(
            take(f.host->sounds()).influences[0].instance == 9 && heard.count == 8 && heard.sounds[0].point == R::Point{22, 0} &&
            heard.sounds[7].point == R::Point{119, 0}
        );
    });
    test("distinct active listener ceiling releases on exact retirement", [] {
        Fixture f;
        std::array<R::ActorId, 5> actors{f.a, f.b};
        auto b = f.host->snapshot().actors[f.b.slot].data;
        b.eligible = true;
        ok(f.host->update_actor(f.b, b));
        for (unsigned i = 2; i < 5; ++i)
            actors[i] = take(f.host->add_actor({{static_cast<std::int32_t>(30 + i), 0}, 50}));
        for (unsigned i = 0; i < 4; ++i)
            f.cast(plan(verse(sound())), {f.caster, actors[i], {}});
        f.unchanged([&] { refuses(f.host->prepare(plan(verse(sound())), {f.caster, actors[4], {}}), R::Error::SoundLimit); });
        auto old = f.listener(f.a);
        ok(f.host->remove_actor(f.a));
        f.cast(plan(verse(sound())), {f.caster, actors[4], {}});
        refuses(f.host->hear(old), R::Error::MissingActor);
        check(take(f.host->sounds()).instances_issued == 5);
    });
    test("downward zero caps and upward bounds", [] {
        for (unsigned field = 0; field < 3; ++field) {
            R::HostLimits l;
            if (field == 0)
                l.sounds = 0;
            if (field == 1)
                l.sound_listeners = 0;
            if (field == 2)
                l.max_sound_instance = 0;
            Fixture f(l);
            f.unchanged([&] {
                refuses(
                    f.host->prepare(plan(verse(sound())), {f.caster, f.a, {}}), field == 2 ? R::Error::InstanceLimit : R::Error::SoundLimit
                );
            });
            check(f.hear(f.a).count == 0);
        }
        for (unsigned field = 0; field < 3; ++field) {
            R::HostLimits l;
            if (field == 0)
                l.sounds = 9;
            if (field == 1)
                l.sound_listeners = 5;
            if (field == 2)
                l.max_sound_instance = 1000001;
            refuses(R::ReferenceHost::create(game(), l), R::Error::InvalidLimits);
        }
    });
    test("exact onset expiry and dated observation ownership", [] {
        Fixture f;
        auto a = f.listener(f.a);
        take(f.host->advance_to(50));
        f.emit();
        auto old = take(f.host->hear(a));
        check(
            old.tick == 50 && take(f.host->sounds()).influences[0].onset_tick == 50 &&
            take(f.host->sounds()).influences[0].until_tick == 250
        );
        take(f.host->advance_to(249));
        check(take(f.host->hear(a)).count == 1);
        take(f.host->advance_to(250));
        check(take(f.host->hear(a)).count == 0 && old.count == 1 && old.tick == 50);
    });
    test("original plan survives source lifetime until last sound expiry", [] {
        Fixture f;
        std::weak_ptr<const Plan> weak;
        {
            auto p = plan(verse(sound()));
            weak = p;
            f.cast(p, {f.caster, f.a, {}});
        }
        check(!weak.expired());
        auto heard = f.hear(f.a);
        take(f.host->advance_to(200));
        check(weak.expired() && heard.count == 1 && f.hear(f.a).count == 0);
    });
    test("dead listener and removed reused generations", [] {
        Fixture f;
        f.emit();
        auto ref = f.listener(f.a);
        auto actor = f.host->snapshot().actors[f.a.slot].data;
        actor.health = 0;
        ok(f.host->update_actor(f.a, actor));
        refuses(f.host->hear(ref), R::Error::Permission);
        refuses(f.host->bind_listener(f.a), R::Error::Permission);
        actor.health = 50;
        ok(f.host->update_actor(f.a, actor));
        check(take(f.host->hear(ref)).count == 1);
        ok(f.host->remove_actor(f.a));
        auto next = take(f.host->add_actor({{20, 0}, 60}));
        check(next.slot == f.a.slot && next.generation == 2 && f.hear(next).count == 0);
        f.cast(plan(verse(sound())), {f.caster, next, {}});
        refuses(f.host->hear(ref), R::Error::MissingActor);
        check(take(f.host->sounds()).influences[0].instance == 2);
    });
    test("accepted touch captures current point and consumes exact charge once", [] {
        Fixture f;
        auto rune = take(f.host->install(plan(composed(true)), f.caster));
        auto event = take(f.host->admit_touch(17, rune, f.a));
        auto actor = f.host->snapshot().actors[f.a.slot].data;
        actor.position = {2100, 30};
        ok(f.host->update_actor(f.a, actor));
        take(f.host->touch(event));
        auto row = take(f.host->sounds()).influences[0];
        check(
            row.source == R::ActivationSource{{}, 0, rune, 17} && row.effect_index == 1 && row.point == R::Point{100, 30} &&
            f.hear(f.b).count == 0 && f.host->snapshot().actors[f.a.slot].data.health == 53 && f.host->items().items[rune.slot].charges == 0
        );
        f.unchanged([&] { refuses(f.host->touch(event), R::Error::RepeatedEvent); });
    });
    test("refused touch preserves heal sound charge and reusable event", [] {
        R::HostLimits l;
        l.sounds = 1;
        Fixture f(l);
        f.emit();
        auto rune = take(f.host->install(plan(composed(true)), f.caster));
        auto event = take(f.host->admit_touch(17, rune, f.a));
        f.unchanged([&] { refuses(f.host->touch(event), R::Error::SoundLimit); });
        check(!f.host->items().ledger[0].consumed && f.host->items().items[rune.slot].charges == 1);
        take(f.host->advance_to(200));
        take(f.host->touch(event));
        check(
            f.host->snapshot().actors[f.a.slot].data.health == 53 && take(f.host->sounds()).instances_issued == 2 &&
            f.host->items().ledger[0].consumed
        );
    });
    test("touch live schema eligibility foreign and disabled event", [] {
        Fixture f, other;
        auto rune = take(f.host->install(plan(composed(true)), f.caster));
        auto event = take(f.host->admit_touch(17, rune, f.a));
        refuses(other.host->touch(event), R::Error::ForeignEvent);
        ok(f.host->enable_item(rune, false));
        f.unchanged([&] { refuses(f.host->touch(event), R::Error::DisabledItem); });
        ok(f.host->enable_item(rune, true));
        auto actor = f.host->snapshot().actors[f.a.slot].data;
        actor.willing = false;
        ok(f.host->update_actor(f.a, actor));
        f.unchanged([&] { refuses(f.host->touch(event), R::Error::Contact); });
        actor.willing = true;
        ok(f.host->update_actor(f.a, actor));
        auto c = game();
        c.operations[3].sound_kinds = 0;
        ok(f.host->replace_catalog(c));
        f.unchanged([&] { refuses(f.host->touch(event), R::Error::SchemaMismatch); });
        ok(f.host->replace_catalog(game()));
        take(f.host->touch(event));
    });
    test("delayed burst pays once and captures listener at activation", [] {
        Fixture f;
        auto p = plan(burst(sound()));
        f.cast(p, {f.caster, {}, R::Point{20, 0}});
        take(f.host->advance_to(99));
        auto actor = f.host->snapshot().actors[f.a.slot].data;
        actor.position = {30, 0};
        ok(f.host->update_actor(f.a, actor));
        take(f.host->advance_to(100));
        auto row = take(f.host->sounds()).influences[0];
        check(
            row.source == R::ActivationSource{f.caster, 1} && row.onset_tick == 100 && row.until_tick == 300 &&
            row.point == R::Point{-1970, 0} && f.host->snapshot().actors[0].data.mana == 990
        );
    });
    test("late multi-listener failure rolls back earlier composed damage", [] {
        R::HostLimits l;
        l.max_sound_instance = 1;
        Fixture f(l);
        auto b = f.host->snapshot().actors[f.b.slot].data;
        b.eligible = true;
        ok(f.host->update_actor(f.b, b));
        auto text = burst("fire damage 4 dp instant");
        text.insert(text.rfind("} }"), "on near bridge Common@1 { " + sound() + " } ; ");
        f.cast(plan(text), {f.caster, {}, R::Point{20, 0}});
        f.unchanged([&] { refuses(f.host->advance_to(100), R::Error::InstanceLimit); });
        check(f.host->snapshot().tick == 0 && f.host->snapshot().actors[f.b.slot].data.health == 60);
        ok(f.host->remove_actor(f.b));
        take(f.host->advance_to(100));
        check(f.host->snapshot().actors[f.a.slot].data.health == 46 && take(f.host->sounds()).instances_issued == 1);
    });
    test("late advance failure restores earlier expiry and staged activation", [] {
        Fixture f;
        f.emit("0 mm 0 mm", "500 ms");
        auto p = plan(burst(sound()));
        f.cast(p, {f.caster, {}, R::Point{20, 0}});
        f.cast(p, {f.caster, {}, R::Point{9000, 0}});
        f.unchanged([&] { refuses(f.host->advance_to(100), R::Error::EmptySelection); });
        check(take(f.host->sounds()).instances_issued == 1 && f.host->snapshot().tick == 0);
        take(f.host->cancel(3));
        take(f.host->advance_to(100));
        check(take(f.host->sounds()).instances_issued == 2 && f.hear(f.a).count == 1 && f.host->snapshot().actors[0].data.mana == 970);
    });
    test("checkpoint owns active pending item plans and renews all input authority", [] {
        R::Checkpoint cp;
        R::ActorId caster, a, b;
        R::ListenerRef old_listener;
        R::TouchEvent old_event;
        R::Ticket old_ticket;
        {
            Fixture f;
            caster = f.caster;
            a = f.a;
            b = f.b;
            f.emit();
            old_listener = f.listener(a);
            f.cast(plan(verse("multiply speed by 750 permille for 3000 ms")), {caster, a, {}});
            f.cast(plan(burst(sound())), {caster, {}, R::Point{20, 0}});
            auto rune = take(f.host->install(plan(composed(true)), caster));
            old_event = take(f.host->admit_touch(17, rune, a));
            old_ticket = take(f.host->prepare(plan(verse(sound())), {caster, a, {}}));
            cp = take(f.host->checkpoint());
        }
        auto host = take(R::ReferenceHost::create(game()));
        ok(host->restore(cp));
        refuses(host->hear(old_listener), R::Error::ForeignCapability);
        refuses(host->release(old_ticket), R::Error::ForeignTicket);
        refuses(host->touch(old_event), R::Error::ForeignEvent);
        auto fresh = take(host->bind_listener(a));
        check(take(host->hear(fresh)).count == 1 && take(host->hear(take(host->bind_listener(b)))).count == 0);
        take(host->touch(take(host->resume_touch(17))));
        take(host->advance_to(100));
        check(
            take(host->hear(fresh)).count == 3 && host->snapshot().actors[a.slot].data.health == 53 &&
            host->snapshot().actors[a.slot].slow == R::Slow{750, 300}
        );
        ok(host->restore(cp));
        refuses(host->hear(fresh), R::Error::ForeignCapability);
        check(take(host->sounds()).instances_issued == 1 && host->snapshot().actors[a.slot].data.health == 50);
    });
    test("departed sources retain provenance with sound checkpoint", [] {
        Fixture f;
        f.emit();
        auto rune = take(f.host->install(plan(verse(sound(), true)), f.caster));
        take(f.host->touch(take(f.host->admit_touch(17, rune, f.a))));
        ok(f.host->remove_item(rune));
        ok(f.host->remove_actor(f.caster));
        auto cp = take(f.host->checkpoint());
        ok(f.host->restore(cp));
        check(
            f.hear(f.a).count == 2 && take(f.host->sounds()).influences[0].source.caster == f.caster &&
            take(f.host->sounds()).influences[1].source.item == rune
        );
    });
    test("restore refusal preserves usable current listener authority", [] {
        Fixture f;
        f.emit();
        auto ref = f.listener(f.a);
        auto cp = take(f.host->checkpoint());
        auto c = game();
        c.operations[3].sound_kinds = 0;
        ok(f.host->replace_catalog(c));
        f.unchanged([&] { refuses(f.host->checkpoint(), R::Error::SchemaMismatch); });
        f.unchanged([&] { refuses(f.host->restore(cp), R::Error::IncompatibleCheckpoint); });
        check(take(f.host->hear(ref)).count == 1);
        R::HostLimits l;
        l.sounds = 7;
        Fixture other(l);
        other.unchanged([&] { refuses(other.host->restore(cp), R::Error::IncompatibleCheckpoint); });
    });
    test("instance time and late action horizons preserve authority", [] {
        R::HostLimits l;
        l.max_sound_instance = 1;
        l.max_tick = 300;
        Fixture f(l);
        f.emit();
        take(f.host->advance_to(200));
        f.unchanged([&] {
            refuses(f.host->prepare(plan(verse(sound("0 mm 0 mm", "10 ms"))), {f.caster, f.a, {}}), R::Error::InstanceLimit);
        });
        l = {};
        l.max_tick = 200;
        Fixture exact(l);
        exact.emit();
        take(exact.host->advance_to(200));
        check(exact.hear(exact.a).count == 0);
        refuses(exact.host->advance_to(201), R::Error::TimeLimit);
        l = {};
        l.max_tick = 199;
        Fixture shorted(l);
        shorted.unchanged([&] {
            refuses(shorted.host->prepare(plan(verse(sound())), {shorted.caster, shorted.a, {}}), R::Error::TimeLimit);
        });
        l = {};
        l.max_action = 0;
        Fixture action(l);
        auto ticket = take(action.host->prepare(plan(verse(sound())), {action.caster, action.a, {}}));
        action.unchanged([&] { refuses(action.host->release(ticket), R::Error::ActionLimit); });
        ok(action.host->discard(ticket));
    });
    test("sound concept book knowledge remains separate from authority", [] {
        auto book = take(L::Spellbook::create());
        ok(book->add("decoy", "Selective footsteps"));
        auto unknown = take(L::Learner::create(all_rules & ~rule_bit(Rule::CommonSound)));
        auto draft = take(book->edit("decoy", verse(sound()), "Exact listener", game(), unknown));
        check(draft->problem() && draft->problem()->code == L::Error::UnknownKnowledge);
        auto known = take(L::Learner::create(all_rules));
        auto accepted = take(book->edit("decoy", verse(sound()), "G0 offset", game(), known))->accepted();
        Fixture f;
        refuses(f.host->release({}), R::Error::InvalidTicket);
        f.cast(accepted->plan(), {f.caster, f.a, {}});
    });
    test("fifteen operation ceiling and sound dispel remains unsupported", [] {
        Catalog c;
        for (unsigned i = 0; i < primitive_count; ++i)
            c.operations.push_back({static_cast<Primitive>(i), "op." + std::to_string(i)});
        check(bool(R::ReferenceHost::create(c)) && bool(compile(verse(sound()), c)));
        c.operations.push_back({Primitive::Sound, "overflow"});
        refuses(R::ReferenceHost::create(c), R::Error::InvalidCatalog);
        check(!compile(
            "spoken release via influence after 0 ms { bind mark = influence captured at release ; together ordered { on mark bridge Common@1 { dispel sound bound influence instant } ; } }",
            game()
        ));
    });
    test("two burst listeners receive separate points and provenance", [] {
        Fixture f;
        auto b = f.host->snapshot().actors[f.b.slot].data;
        b.eligible = true;
        ok(f.host->update_actor(f.b, b));
        f.cast(plan(burst(sound())), {f.caster, {}, R::Point{20, 0}});
        take(f.host->advance_to(100));
        auto a_heard = f.hear(f.a), b_heard = f.hear(f.b);
        auto rows = take(f.host->sounds());
        check(
            a_heard.listener == f.a && b_heard.listener == f.b && a_heard.tick == 100 && b_heard.tick == 100 && a_heard.count == 1 &&
            b_heard.count == 1 && a_heard.sounds[0].point == R::Point{-1980, 0} && b_heard.sounds[0].point == R::Point{-1960, 0}
        );
        check(
            rows.influences[0].instance == 1 && rows.influences[1].instance == 2 &&
            rows.influences[0].source == R::ActivationSource{f.caster, 1} && rows.influences[1].source == R::ActivationSource{f.caster, 1}
        );
    });
    test("overlapping bindings refuse complete sound and health payload", [] {
        Fixture f;
        auto text = burst("heal 3 hp instant", 0);
        text.insert(
            text.find("together ordered"),
            "bind second = eligible actors in G0 disk 100 mm around aim captured at release queried at activation inclusive ; "
        );
        text.insert(text.rfind("} }"), "on near bridge Common@1 { " + sound() + " } ; on second bridge Common@1 { " + sound() + " } ; ");
        auto p = plan(text);
        f.unchanged([&] { refuses(f.host->prepare(p, {f.caster, {}, R::Point{20, 0}}), R::Error::Overlap); });
        check(f.host->snapshot().actors[f.a.slot].data.health == 50 && take(f.host->sounds()).instances_issued == 0);
    });
    test("sampling exact work bound and one short", [] {
        R::HostLimits l;
        l.work = 365; // fixed340 +actor1 +owned output/scratch16 +eight record visits
        Fixture f(l);
        auto ref = f.listener(f.a);
        check(take(f.host->hear(ref)).count == 0);
        l.work = 364;
        Fixture shorted(l);
        auto before = shorted.host->snapshot();
        auto old = shorted.listener(shorted.a);
        refuses(shorted.host->hear(old), R::Error::WorkLimit);
        check(shorted.host->snapshot() == before);
    });
    test("exact application work and one short preserve world", [] {
        Fixture measure;
        auto p = plan(verse(sound()));
        auto work = take(measure.host->preview(p, {measure.caster, measure.a, {}})).usage.work;
        R::HostLimits l;
        l.work = work;
        Fixture exact(l);
        auto t = take(exact.host->prepare(p, {exact.caster, exact.a, {}}));
        check(take(exact.host->release(t))->usage.work == work);
        l.work = work - 1;
        Fixture shorted(l);
        auto before = shorted.host->snapshot();
        auto rows = take(shorted.host->sounds());
        refuses(shorted.host->prepare(p, {shorted.caster, shorted.a, {}}), R::Error::WorkLimit);
        check(before == shorted.host->snapshot() && rows == take(shorted.host->sounds()));
        std::cout << "MEASURE sound application work=" << work << " one-short=" << work - 1 << '\n';
    });
    std::cout << "MEASURE sound row bytes=" << sizeof(R::SoundInfluence) << " snapshot bytes=" << sizeof(R::SoundSnapshot)
              << " hearing bytes=" << sizeof(R::Hearing) << " host bytes=" << sizeof(R::ReferenceHost) << '\n';
    std::cout << passed << " sound cases passed; " << failed << " failed\n";
    return failed ? 1 : 0;
}
