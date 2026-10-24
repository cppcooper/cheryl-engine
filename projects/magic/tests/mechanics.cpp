#include <magic/learning.h>
#include <magic/reference-host.h>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace {
    using namespace Magic;
    namespace R = Magic::Reference;
    namespace L = Magic::Learning;
    int passed = 0, failed = 0;
    void check(bool v, std::string_view why = "literal mismatch") {
        if (!v)
            throw std::runtime_error(std::string(why));
    }
    template <class T, class E> T take(std::expected<T, E> v) {
        if (!v)
            throw std::runtime_error("unexpected refusal");
        return std::move(*v);
    }
    template <class E> void ok(std::expected<void, E> v) {
        check(bool(v), "expected success");
    }
    template <class T, class E, class C> void rejects(const std::expected<T, E>& v, C code) {
        check(!v, "expected refusal");
        check(v.error().code == code, "wrong refusal");
    }
    void test(std::string_view name, const std::function<void()>& body) {
        try {
            body();
            ++passed;
            std::cout << "PASS " << name << '\n';
        } catch (const std::exception& e) {
            ++failed;
            std::cout << "FAIL " << name << ": " << e.what() << '\n';
        }
    }
    Catalog game() {
        Catalog c;
        c.operations = {{Primitive::Heal, "game.heal"},         {Primitive::Fire, "game.fire"},
                        {Primitive::Slow, "game.slow"},         {Primitive::Disguise, "game.disguise"},
                        {Primitive::Discern, "game.discern"},   {Primitive::Ward, "game.ward"},
                        {Primitive::Restrict, "game.restrict"}, {Primitive::Relocate, "game.relocate"},
                        {Primitive::Shatter, "game.shatter"}};
        return c;
    }
    std::string contact(std::string_view clause) {
        return "spoken release via contact after 0 ms {\n  bind hand = willing touched actor at release ;\n  together ordered {\n    on hand bridge Common@1 { " +
               std::string(clause) + " } ;\n  }\n}";
    }
    std::string area(std::string_view clause) {
        return "spoken release via burst after 1000 ms {\n  bind near = eligible actors in G0 disk 250 mm around aim captured at release queried at activation inclusive ;\n  together ordered {\n    on near bridge Common@1 { " +
               std::string(clause) + " } ;\n  }\n}";
    }
    std::string replace(std::string s, std::string_view a, std::string_view b) {
        auto p = s.find(a);
        check(p != s.npos);
        s.replace(p, a.size(), b);
        return s;
    }
    PlanHandle plan(std::string_view source) {
        auto p = compile(source, game());
        if (!p)
            throw std::runtime_error(p.error().message);
        return *p;
    }
    std::string subject_source(std::string_view clause, bool delayed = false) {
        return std::string("spoken release via subject after ") + (delayed ? "1000" : "0") +
               " ms {\n  bind object = object captured at release rechecked at activation ;\n  together ordered {\n    on object bridge Common@1 { " +
               std::string(clause) + " } ;\n  }\n}";
    }
    struct Scene {
        std::unique_ptr<R::ReferenceHost> h;
        R::ActorId caster, target, attacker;
        explicit Scene(R::HostLimits limits = {}) {
            h = take(R::ReferenceHost::create(game(), limits));
            caster = take(h->add_actor({{0, 0}, 100, 100, 1000, 1000, false, true, true}));
            target = take(h->add_actor({{10, 0}, 50, 100, 0, 1000, true, true, false}));
            attacker = take(h->add_actor({{-10, 0}, 100, 100, 0, 1000, true, true, false}));
        }
        R::Observed cast(std::string_view clause, R::ActorId recipient = {}) {
            if (!recipient.generation)
                recipient = target;
            return take(h->release(take(h->prepare(plan(contact(clause)), {caster, recipient, {}}))));
        }
        template <class F> void unchanged(F f) {
            auto w = h->snapshot();
            auto i = h->items();
            auto p = take(h->inspect_perception());
            auto m = take(h->inspect_mechanics());
            f();
            check(
                w == h->snapshot() && i == h->items() && p == take(h->inspect_perception()) && m == take(h->inspect_mechanics()),
                "complete transition preserved"
            );
        }
    };
}
int main() {
    test("ward and restriction exact typed meanings Common and concept requirements", [] {
        auto w = plan(contact("ward absorb 5 dp for 2000 ms"));
        Meaning m;
        m.bindings = {{"hand", Query::WillingTouchedActor, "", 0, BindingTime::Release, BindingTime::Release, true}};
        m.effects = {{Primitive::Ward, "game.ward", 0, 0, 0, Absorption{5, 200}}};
        check(w->meaning() == m && w->common() == contact("ward absorb 5 dp for 2000 ms"));
        check(plan(w->common())->meaning() == m);
        check(w->requirements() == (rule_bit(Rule::TargetBridge) | rule_bit(Rule::CommonWard)));
        m.effects = {{Primitive::Restrict, "game.restrict", 0, 0, 0, IntentRestriction{Intent::Attack, 200}}};
        auto r = plan(contact("forbid attack for 2000 ms"));
        check(r->meaning() == m && plan(r->common())->meaning() == m);
        check(r->requirements() == (rule_bit(Rule::TargetBridge) | rule_bit(Rule::CommonRestrict)));
    });
    test("capacity5 hits3 then4 absorbs3 then2 and passes2 exactly once", [] {
        Scene f;
        auto before = f.h->snapshot();
        check(before.actors[f.target.slot].data.health == 50);
        f.cast("ward absorb 5 dp for 2000 ms");
        R::MechanicsSnapshot literal;
        literal.instances_issued = 1;
        literal.wards[0] = {1, 1, f.target, 5, 200};
        check(take(f.h->inspect_mechanics()) == literal);
        auto hit = take(f.h->try_attack(f.attacker, f.target));
        check(hit->damage == R::DamageResolution{3, 3, 0});
        literal.wards[0].remaining_dp = 2;
        check(hit->mechanics_after == literal && f.h->snapshot().actors[f.target.slot].data.health == 50);
        f.cast("fire damage 4 dp instant");
        literal.wards[0] = {};
        check(take(f.h->inspect_mechanics()) == literal && f.h->snapshot().actors[f.target.slot].data.health == 48);
        check(hit->damage == R::DamageResolution{3, 3, 0} && hit->mechanics_after.wards[0].remaining_dp == 2);
        take(f.h->try_attack(f.attacker, f.target));
        check(f.h->snapshot().actors[f.target.slot].data.health == 45 && f.h->snapshot().actors[f.caster.slot].data.mana == 980);
    });
    test("attack intent restriction gates actual action and exact expiry", [] {
        Scene f;
        f.cast("forbid attack for 2000 ms", f.attacker);
        R::MechanicsSnapshot m;
        m.instances_issued = 1;
        m.restrictions[0] = {1, 1, f.attacker, Intent::Attack, 200};
        check(take(f.h->inspect_mechanics()) == m);
        f.unchanged([&] { rejects(f.h->try_attack(f.attacker, f.target), R::Error::RestrictedIntent); });
        take(f.h->advance_to(199));
        f.unchanged([&] { rejects(f.h->try_attack(f.attacker, f.target), R::Error::RestrictedIntent); });
        take(f.h->advance_to(200));
        auto hit = take(f.h->try_attack(f.attacker, f.target));
        check(hit->damage == R::DamageResolution{3, 0, 3} && f.h->snapshot().actors[f.target.slot].data.health == 47);
    });
    test("ward and restriction interact without changing unrelated state", [] {
        Scene f;
        f.cast("ward absorb 5 dp for 2000 ms");
        f.cast("forbid attack for 1000 ms", f.attacker);
        f.unchanged([&] { rejects(f.h->try_attack(f.attacker, f.target), R::Error::RestrictedIntent); });
        check(take(f.h->inspect_mechanics()).wards[0].remaining_dp == 5);
        take(f.h->advance_to(100));
        take(f.h->try_attack(f.attacker, f.target));
        check(take(f.h->inspect_mechanics()).wards[0].remaining_dp == 2 && f.h->snapshot().actors[f.target.slot].data.health == 50);
    });
    test("current symbolic keys primitive quantities duration permission recheck", [] {
        for (unsigned i = 0; i < 5; ++i) {
            Scene f;
            auto p = plan(contact("ward absorb 5 dp for 2000 ms"));
            auto t = take(f.h->prepare(p, {f.caster, f.target, {}}));
            auto c = game();
            if (i == 0)
                c.operations[5].key = "unknown.ward";
            if (i == 1)
                c.operations[5].max_quantity = 4;
            if (i == 2)
                c.operations[5].max_duration_ticks = 199;
            if (i == 3)
                std::swap(c.operations[5].primitive, c.operations[6].primitive);
            if (i == 4) {
                auto a = f.h->snapshot().actors[f.caster.slot].data;
                a.can_cast = false;
                ok(f.h->update_actor(f.caster, a));
            } else
                ok(f.h->replace_catalog(c));
            f.unchanged([&] { rejects(f.h->release(t), i == 4 ? R::Error::Permission : R::Error::SchemaMismatch); });
        }
    });
    test("overlap refuses and refused action leaves valid earlier ticket usable", [] {
        Scene f;
        f.cast("ward absorb 5 dp for 2000 ms");
        f.unchanged([&] {
            rejects(f.h->prepare(plan(contact("ward absorb 7 dp for 2000 ms")), {f.caster, f.target, {}}), R::Error::ActiveWard);
        });
        f.cast("forbid attack for 1000 ms", f.attacker);
        f.unchanged([&] {
            rejects(f.h->prepare(plan(contact("forbid attack for 2000 ms")), {f.caster, f.attacker, {}}), R::Error::Overlap);
        });
        take(f.h->advance_to(200));
        f.cast("ward absorb 7 dp for 2000 ms");
        check(take(f.h->inspect_mechanics()).wards[0].remaining_dp == 7);
    });
    test("new actor generations do not inherit expired or removed contributions", [] {
        Scene f;
        f.cast("ward absorb 5 dp for 2000 ms");
        ok(f.h->remove_actor(f.target));
        auto replacement = take(f.h->add_actor({{10, 0}, 50, 100, 0, 1000, true, true, false}));
        check(replacement.slot == f.target.slot && replacement.generation == 2);
        check(take(f.h->inspect_mechanics()).wards[0].instance == 0);
        take(f.h->try_attack(f.attacker, replacement));
        check(f.h->snapshot().actors[replacement.slot].data.health == 47);
        take(f.h->advance_to(200));
        check(f.h->snapshot().actors[replacement.slot].present);
    });
    test("pending audience and capacity resolve at activation paid failure preserved", [] {
        R::HostLimits l;
        l.wards = 0;
        Scene f(l);
        auto p = plan(area("ward absorb 5 dp for 2000 ms"));
        take(f.h->release(take(f.h->prepare(p, {f.caster, {}, R::Point{}}))));
        f.unchanged([&] { rejects(f.h->advance_to(100), R::Error::MechanicLimit); });
        check(f.h->snapshot().pending[0].present && f.h->snapshot().actors[f.caster.slot].data.mana == 990);
        take(f.h->cancel(1));
        check(f.h->snapshot().actors[f.caster.slot].data.mana == 990);
    });
    test("exact4 ward slots and one-short activation rollback", [] {
        for (std::size_t cap : {3u, 4u}) {
            R::HostLimits l;
            l.wards = cap;
            Scene f(l);
            auto attacker = f.h->snapshot().actors[f.attacker.slot].data;
            attacker.eligible = false;
            ok(f.h->update_actor(f.attacker, attacker));
            for (int x : {20, 30, 40})
                take(f.h->add_actor({{x, 0}, 50, 100, 0, 1000, true, true, false}));
            auto p = plan(area("ward absorb 5 dp for 2000 ms"));
            take(f.h->release(take(f.h->prepare(p, {f.caster, {}, R::Point{}}))));
            if (cap == 3)
                f.unchanged([&] { rejects(f.h->advance_to(100), R::Error::MechanicLimit); });
            else {
                take(f.h->advance_to(100));
                auto m = take(f.h->inspect_mechanics());
                check(m.instances_issued == 4 && m.wards[3].remaining_dp == 5);
            }
        }
    });
    test("new identity horizon exhaustion refuses without wrapping", [] {
        R::HostLimits l;
        l.max_mechanic_instance = 1;
        Scene f(l);
        f.cast("ward absorb 5 dp for 2000 ms");
        take(f.h->advance_to(200));
        f.unchanged([&] {
            rejects(f.h->prepare(plan(contact("forbid attack for 2000 ms")), {f.caster, f.target, {}}), R::Error::MechanicLimit);
        });
        check(take(f.h->inspect_mechanics()).instances_issued == 1);
    });
    test("owned checkpoints retain partially spent wards and restrictions", [] {
        R::Checkpoint cp;
        R::ActorId attacker, target;
        std::weak_ptr<const Plan> weak;
        {
            Scene f;
            attacker = f.attacker;
            target = f.target;
            auto source = contact("ward absorb 5 dp for 2000 ms");
            auto p = plan(source);
            weak = p;
            take(f.h->release(take(f.h->prepare(p, {f.caster, f.target, {}}))));
            take(f.h->try_attack(f.attacker, f.target));
            f.cast("forbid attack for 1000 ms", f.attacker);
            cp = take(f.h->checkpoint());
        }
        check(!weak.expired());
        auto h = take(R::ReferenceHost::create(game()));
        ok(h->restore(cp));
        check(take(h->inspect_mechanics()).wards[0].remaining_dp == 2);
        rejects(h->try_attack(attacker, target), R::Error::RestrictedIntent);
        take(h->advance_to(100));
        auto hit = take(h->try_attack(attacker, target));
        check(hit->damage == R::DamageResolution{3, 2, 1} && h->snapshot().actors[target.slot].data.health == 49);
        cp = {};
        check(weak.expired());
    });
    test("source dimensions ranges unsupported bundles owned diagnostics", [] {
        for (auto text :
             {"ward absorb 5 hp for 2000 ms", "ward absorb 0 dp for 2000 ms", "ward absorb 5 dp for 1 ms", "forbid walk for 2000 ms",
              "forbid attack for 0 ms"}) {
            auto r = compile(contact(text), game());
            check(!r && r.error().span.length > 0 && !r.error().message.empty());
        }
        auto s = contact("ward absorb 5 dp for 2000 ms");
        s.insert(s.rfind("  }"), "on hand bridge Common@1 { heal 1 hp instant } ;\n");
        rejects(compile(s, game()), Code::UnsupportedComposition);
        auto engraved = replace(
            contact("ward absorb 5 dp for 2000 ms"), "spoken release via contact", "engraved item-touch charges 1 release via item-event"
        );
        check(!compile(engraved, game()));
    });
    test("default foreign stale repeated normal live tickets still gate mechanics", [] {
        Scene f, g;
        auto p = plan(contact("ward absorb 5 dp for 2000 ms"));
        auto t = take(g.h->prepare(p, {g.caster, g.target, {}}));
        f.unchanged([&] {
            rejects(f.h->release({}), R::Error::InvalidTicket);
            rejects(f.h->release(t), R::Error::ForeignTicket);
        });
        t = take(f.h->prepare(p, {f.caster, f.target, {}}));
        auto a = f.h->snapshot().actors[f.target.slot].data;
        a.health = 49;
        ok(f.h->update_actor(f.target, a));
        rejects(f.h->release(t), R::Error::StaleTicket);
        t = take(f.h->prepare(p, {f.caster, f.target, {}}));
        take(f.h->release(t));
        rejects(f.h->release(t), R::Error::ConsumedTicket);
    });
    test("new finite cap expansion refuses and exact mana9 versus10", [] {
        R::HostLimits l;
        l.wards = 5;
        rejects(R::ReferenceHost::create(game(), l), R::Error::InvalidLimits);
        l = {};
        l.restrictions = 5;
        rejects(R::ReferenceHost::create(game(), l), R::Error::InvalidLimits);
        for (unsigned mana : {9, 10}) {
            Scene f;
            auto a = f.h->snapshot().actors[f.caster.slot].data;
            a.mana = mana;
            ok(f.h->update_actor(f.caster, a));
            if (mana == 9)
                f.unchanged([&] {
                    rejects(f.h->prepare(plan(contact("ward absorb 5 dp for 2000 ms")), {f.caster, f.target, {}}), R::Error::Resources);
                });
            else {
                f.cast("ward absorb 5 dp for 2000 ms");
                check(f.h->snapshot().actors[f.caster.slot].data.mana == 0);
            }
        }
    });
    test("relocation and shatter have exact typed roles and Common roundtrip", [] {
        Meaning expected;
        expected.bindings = {{"hand", Query::WillingTouchedActor, "", 0, BindingTime::Release, BindingTime::Release, true}};
        expected.effects = {{Primitive::Relocate, "game.relocate", 0, 0, 0, Relocation{{-300, 400}}}};
        auto p = plan(contact("relocate to G0 point -300 mm 400 mm instant"));
        check(
            p->meaning() == expected && p->common() == contact("relocate to G0 point -300 mm 400 mm instant") &&
            plan(p->common())->meaning() == expected
        );
        expected.delivery = Delivery::Subject;
        expected.bindings = {{"object", Query::CapturedObject, "", 0, BindingTime::Release, BindingTime::Activation, true}};
        expected.effects = {{Primitive::Shatter, "game.shatter", 0, 0, 0, ObjectShatter{SubjectRole::CapturedObject}}};
        p = plan(subject_source("shatter object instant"));
        check(
            p->meaning() == expected && p->common() == subject_source("shatter object instant") && plan(p->common())->meaning() == expected
        );
        check(p->requirements() == (rule_bit(Rule::TargetBridge) | rule_bit(Rule::CommonShatter)));
    });
    test("real wall blocks relocation until shatter removes object without injuring actor", [] {
        Scene f;
        auto wall = take(f.h->add_object({Appearance::StoneWall, false, {300, 0}}));
        auto old = take(f.h->inspect_perception());
        auto relocation = plan(contact("relocate to G0 point 300 mm 0 mm instant"));
        f.unchanged([&] { rejects(f.h->prepare(relocation, {f.caster, f.target, {}}), R::Error::DestinationBlocked); });
        take(f.h->release(take(f.h->prepare(plan(subject_source("shatter object instant")), {f.caster, {}, {}, wall}))));
        auto destroyed = take(f.h->inspect_perception());
        check(destroyed.objects[wall.slot] == R::ObjectView{wall});
        auto before = f.h->snapshot();
        auto moved = take(f.h->release(take(f.h->prepare(relocation, {f.caster, f.target, {}}))));
        auto expected = before;
        expected.revision++;
        expected.actions_issued++;
        expected.actors[f.caster.slot].data.mana -= 10;
        expected.actors[f.target.slot].data.position = {300, 0};
        check(
            moved->after == expected && f.h->snapshot() == expected && old.objects[wall.slot].present &&
            old.objects[wall.slot].data.position == R::Point{300, 0}
        );
        check(f.h->snapshot().actors[f.target.slot].data.health == 50 && f.h->snapshot().actors[f.caster.slot].data.mana == 980);
    });
    test("relocation does not walk intermediate blockers and includes extreme coordinates", [] {
        Scene f;
        take(f.h->add_object({Appearance::StoneWall, false, {150, 0}}));
        f.cast("relocate to G0 point 300 mm 0 mm instant");
        check(f.h->snapshot().actors[f.target.slot].data.position == R::Point{300, 0});
        auto caster = f.h->snapshot().actors[f.caster.slot].data;
        caster.eligible = true;
        ok(f.h->update_actor(f.caster, caster));
        auto self = plan(contact("relocate to G0 point -1000000 mm 1000000 mm instant"));
        take(f.h->release(take(f.h->prepare(self, {f.caster, f.caster, {}}))));
        check(f.h->snapshot().actors[f.caster.slot].data.position == R::Point{-1000000, 1000000});
    });
    test("relocation refuses occupied actor and opaque wall but passable object is allowed", [] {
        Scene f;
        auto p = plan(contact("relocate to G0 point -10 mm 0 mm instant"));
        f.unchanged([&] { rejects(f.h->prepare(p, {f.caster, f.target, {}}), R::Error::DestinationBlocked); });
        auto arch = take(f.h->add_object({Appearance::OpenArch, true, {300, 0}}));
        f.cast("relocate to G0 point 300 mm 0 mm instant");
        check(take(f.h->inspect_perception()).objects[arch.slot].present);
    });
    test("attack restriction permits actual relocation of the restricted actor", [] {
        Scene f;
        f.cast("forbid attack for 2000 ms", f.attacker);
        f.cast("relocate to G0 point 50 mm 0 mm instant", f.attacker);
        check(f.h->snapshot().actors[f.attacker.slot].data.position == R::Point{50, 0});
        f.unchanged([&] { rejects(f.h->try_attack(f.attacker, f.target), R::Error::RestrictedIntent); });
        take(f.h->advance_to(200));
        take(f.h->try_attack(f.attacker, f.target));
        check(f.h->snapshot().actors[f.target.slot].data.health == 47);
    });
    test("new role wrong dimension exact coordinates overflow and unsupported envelopes refuse", [] {
        for (auto clause :
             {"relocate to G0 point 1.1 mm 0 mm instant", "relocate to G0 point 1000001 mm 0 mm instant",
              "relocate to G0 point 1 hp 0 mm instant", "relocate to G0 point 1 px 0 mm instant", "relocate to G9 point 0 mm 0 mm instant"})
            check(!compile(contact(clause), game()));
        rejects(compile(contact("shatter object instant"), game()), Code::RoleMismatch);
        rejects(compile(subject_source("heal 1 hp instant"), game()), Code::RoleMismatch);
        rejects(compile(area("relocate to G0 point 0 mm 0 mm instant"), game()), Code::UnsupportedCombination);
        auto s = subject_source("shatter object instant");
        s.insert(s.rfind("  }"), "on object bridge Common@1 { shatter object instant } ;\n");
        check(!compile(s, game()));
    });
    test("subject delivery live keys capabilities and current forms recheck before debit", [] {
        for (unsigned i = 0; i < 4; ++i) {
            Scene f;
            auto object = take(f.h->add_object({Appearance::StoneWall, false, {300, 0}}));
            auto p = plan(subject_source("shatter object instant"));
            auto t = take(f.h->prepare(p, {f.caster, {}, {}, object}));
            auto c = game();
            if (i == 0)
                c.operations[8].key = "changed.shatter";
            if (i == 1)
                c.subject = false;
            if (i == 2)
                c.operations[8].appearances = 1;
            if (i == 3)
                std::swap(c.operations[8].primitive, c.operations[7].primitive);
            ok(f.h->replace_catalog(c));
            f.unchanged([&] { rejects(f.h->release(t), R::Error::SchemaMismatch); });
        }
    });
    test("captured delayed subject cannot retarget replacement or silently refund", [] {
        Scene f;
        auto object = take(f.h->add_object({Appearance::StoneWall, false, {300, 0}}));
        auto p = plan(subject_source("shatter object instant", true));
        take(f.h->release(take(f.h->prepare(p, {f.caster, {}, {}, object}))));
        ok(f.h->remove_object(object));
        auto replacement = take(f.h->add_object({Appearance::OpenArch, true, {300, 0}}));
        f.unchanged([&] { rejects(f.h->advance_to(100), R::Error::MissingObject); });
        check(
            replacement.generation == 2 && take(f.h->inspect_perception()).objects[0].id == replacement &&
            f.h->snapshot().actors[f.caster.slot].data.mana == 990
        );
        take(f.h->cancel(1));
        check(take(f.h->inspect_perception()).objects[0].present);
    });
    test("pending shatter checkpoint owns source captured subject and exact due", [] {
        R::Checkpoint cp;
        R::ObjectId object;
        std::weak_ptr<const Plan> weak;
        R::Ticket old;
        {
            Scene f;
            object = take(f.h->add_object({Appearance::StoneWall, false, {300, 0}}));
            auto text = subject_source("shatter object instant", true);
            auto p = plan(text);
            weak = p;
            old = take(f.h->prepare(p, {f.caster, {}, {}, object}));
            take(f.h->release(old));
            cp = take(f.h->checkpoint());
        }
        auto h = take(R::ReferenceHost::create(game()));
        ok(h->restore(cp));
        rejects(h->release(old), R::Error::ForeignTicket);
        cp = {};
        check(!weak.expired());
        take(h->advance_to(99));
        check(take(h->inspect_perception()).objects[object.slot].present);
        take(h->advance_to(100));
        check(!take(h->inspect_perception()).objects[object.slot].present && weak.expired());
        take(h->advance_to(101));
        check(!take(h->inspect_perception()).objects[object.slot].present);
    });
    test("shatter removes only its disguise and preserves other subjects history and health", [] {
        Scene f;
        auto a = take(f.h->add_object({Appearance::StoneWall, false, {300, 0}}));
        auto b = take(f.h->add_object({Appearance::OpenArch, true, {400, 0}}));
        auto disguise = plan(
            "spoken release via burst after 0 ms { bind near = eligible actors in G0 disk 250 mm around aim captured at release queried at activation inclusive ; together ordered { on near bridge Common@1 { show subject captured at release rechecked at activation as stone-wall for 2000 ms to this audience frozen at activation } ; } }"
        );
        take(f.h->release(take(f.h->prepare(disguise, {f.caster, {}, R::Point{}, b}))));
        take(f.h->release(take(f.h->prepare(plan(subject_source("shatter object instant")), {f.caster, {}, {}, a}))));
        auto p = take(f.h->inspect_perception());
        check(!p.objects[a.slot].present && p.objects[b.slot].present && p.disguises[0].subject == b && p.disguises[0].instance == 1);
        check(f.h->snapshot().actors[f.target.slot].data.health == 50);
    });
    test("relocation occupancy selector work exact21 and one-short20", [] {
        for (std::size_t cap : {20u, 21u}) {
            R::HostLimits l;
            l.selector_work = cap;
            Scene f(l);
            auto p = plan(contact("relocate to G0 point 300 mm 0 mm instant"));
            if (cap == 20)
                f.unchanged([&] { rejects(f.h->prepare(p, {f.caster, f.target, {}}), R::Error::SelectorLimit); });
            else {
                auto r = take(f.h->release(take(f.h->prepare(p, {f.caster, f.target, {}}))));
                check(r->usage.selector_work == 21 && r->usage.selected_targets == 1);
            }
        }
    });
    std::cout << passed << " M7 mechanics cases passed; " << failed << " failed.\n";
    return failed ? 1 : 0;
}
