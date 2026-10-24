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
                        {Primitive::Shatter, "game.shatter"},   {Primitive::Transmute, "game.transmute"},
                        {Primitive::Create, "game.create"}};
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
    test("material and creation typed meanings exact Common and independent parameters", [] {
        auto text = subject_source("transmute object to open-arch for 2000 ms");
        auto p = plan(text);
        Meaning m;
        m.delivery = Delivery::Subject;
        m.bindings = {{"object", Query::CapturedObject, "", 0, BindingTime::Release, BindingTime::Activation, true}};
        m.effects = {
            {Primitive::Transmute, "game.transmute", 0, 0, 0, Transmutation{SubjectRole::CapturedObject, Appearance::OpenArch, 200}}
        };
        check(p->meaning() == m && p->common() == text && plan(p->common())->meaning() == m);
        auto c = plan(contact("create stone-wall at G0 point -300 mm 400 mm for 1500 ms owned by this actor"));
        m.delivery = Delivery::Contact;
        m.bindings = {{"hand", Query::WillingTouchedActor, "", 0, BindingTime::Release, BindingTime::Release, true}};
        m.effects = {
            {Primitive::Create, "game.create", 0, 0, 0, Creation{Appearance::StoneWall, {-300, 400}, 150, OwnershipRole::SelectedActor}}
        };
        check(c->meaning() == m && plan(c->common())->meaning() == m);
        check(c->requirements() == (rule_bit(Rule::TargetBridge) | rule_bit(Rule::CommonCreate)));
    });
    test("physical transmutation exact identity passability onset and expiry", [] {
        Scene f;
        auto wall = take(f.h->add_object({Appearance::StoneWall, false, {300, 0}}));
        auto old = take(f.h->inspect_perception());
        take(f.h->release(take(f.h->prepare(plan(subject_source("transmute object to open-arch for 2000 ms")), {f.caster, {}, {}, wall}))));
        R::MechanicsSnapshot expected;
        expected.instances_issued = 1;
        expected.transmutations[0] = {1, 1, wall, Appearance::StoneWall, false, Appearance::OpenArch, 0, 200};
        check(take(f.h->inspect_mechanics()) == expected);
        check(take(f.h->inspect_perception()).objects[0] == R::ObjectView{wall, true, {Appearance::OpenArch, true, {300, 0}}, 5});
        take(f.h->advance_to(199));
        check(take(f.h->inspect_perception()).objects[0].data.passable);
        take(f.h->advance_to(200));
        expected.transmutations[0] = {};
        check(take(f.h->inspect_mechanics()) == expected);
        check(take(f.h->inspect_perception()).objects[0] == R::ObjectView{wall, true, {Appearance::StoneWall, false, {300, 0}}, 7});
        check(old.objects[0].data == R::ObjectSpec{Appearance::StoneWall, false, {300, 0}});
    });
    test("transmuted wall opens relocation while illusion still changes only perception", [] {
        Scene f;
        auto wall = take(f.h->add_object({Appearance::StoneWall, false, {300, 0}}));
        auto disguise = plan(
            "spoken release via burst after 0 ms { bind near = eligible actors in G0 disk 250 mm around aim captured at release queried at activation inclusive ; together ordered { on near bridge Common@1 { show subject captured at release rechecked at activation as stone-wall for 2000 ms to this audience frozen at activation } ; } }"
        );
        take(f.h->release(take(f.h->prepare(disguise, {f.caster, {}, R::Point{}, wall}))));
        take(f.h->release(take(f.h->prepare(plan(subject_source("transmute object to open-arch for 1000 ms")), {f.caster, {}, {}, wall}))));
        check(
            take(f.h->perceive(f.target, wall)).form == Appearance::StoneWall && take(f.h->inspect_perception()).objects[0].data.passable
        );
        f.cast("relocate to G0 point 300 mm 0 mm instant");
        check(
            f.h->snapshot().actors[f.target.slot].data.position == R::Point{300, 0} &&
            f.h->snapshot().actors[f.target.slot].data.health == 50
        );
        take(f.h->advance_to(100));
        check(
            !take(f.h->inspect_perception()).objects[0].data.passable &&
            f.h->snapshot().actors[f.target.slot].data.position == R::Point{300, 0}
        );
    });
    test("owner physical edit supersedes transmutation without restoring older fields", [] {
        Scene f;
        auto wall = take(f.h->add_object({Appearance::StoneWall, false, {300, 0}}));
        take(f.h->release(take(f.h->prepare(plan(subject_source("transmute object to open-arch for 2000 ms")), {f.caster, {}, {}, wall}))));
        ok(f.h->update_object(wall, {Appearance::OpenArch, true, {400, 0}}));
        check(take(f.h->inspect_mechanics()).transmutations[0].instance == 0);
        auto edited = take(f.h->inspect_perception());
        take(f.h->advance_to(200));
        check(take(f.h->inspect_perception()).objects[0] == edited.objects[0]);
    });
    test("shatter during transmutation cannot resurrect or remove a replacement", [] {
        Scene f;
        auto wall = take(f.h->add_object({Appearance::StoneWall, false, {300, 0}}));
        take(f.h->release(take(f.h->prepare(plan(subject_source("transmute object to open-arch for 2000 ms")), {f.caster, {}, {}, wall}))));
        take(f.h->release(take(f.h->prepare(plan(subject_source("shatter object instant")), {f.caster, {}, {}, wall}))));
        auto replacement = take(f.h->add_object({Appearance::StoneWall, false, {600, 0}}));
        check(replacement.generation == 2);
        take(f.h->advance_to(200));
        check(
            take(f.h->inspect_perception()).objects[0].id == replacement &&
            take(f.h->inspect_perception()).objects[0].data.position == R::Point{600, 0}
        );
    });
    test("creation adopts fresh real object owner lifetime payment atomically", [] {
        Scene f;
        auto before = f.h->snapshot();
        auto report = f.cast("create stone-wall at G0 point 300 mm 0 mm for 2000 ms owned by this actor");
        auto after = before;
        after.actions_issued = 1;
        after.revision = 4;
        after.actors[f.caster.slot].data.mana = 990;
        check(report->after == after);
        R::MechanicsSnapshot m;
        m.instances_issued = 1;
        m.creations[0] = {1, 1, {0, 1}, f.target, 0, 200};
        check(take(f.h->inspect_mechanics()) == m);
        check(take(f.h->inspect_perception()).objects[0] == R::ObjectView{{0, 1}, true, {Appearance::StoneWall, false, {300, 0}}, 4});
        f.unchanged([&] {
            rejects(
                f.h->prepare(plan(contact("relocate to G0 point 300 mm 0 mm instant")), {f.caster, f.target, {}}),
                R::Error::DestinationBlocked
            );
        });
        take(f.h->advance_to(199));
        check(take(f.h->inspect_perception()).objects[0].present);
        take(f.h->advance_to(200));
        check(
            !take(f.h->inspect_perception()).objects[0].present && take(f.h->inspect_mechanics()).creations[0].instance == 0 &&
            f.h->snapshot().actors[f.target.slot].data.health == 50
        );
    });
    test("only recorded living creation owner dismisses; replacement survives old cleanup", [] {
        Scene f;
        f.cast("create stone-wall at G0 point 300 mm 0 mm for 2000 ms owned by this actor");
        R::ObjectId created{0, 1};
        f.unchanged([&] { rejects(f.h->dismiss_created(f.caster, created), R::Error::Permission); });
        ok(f.h->dismiss_created(f.target, created));
        auto replacement = take(f.h->add_object({Appearance::OpenArch, true, {400, 0}}));
        take(f.h->advance_to(200));
        check(replacement == R::ObjectId{0, 2} && take(f.h->inspect_perception()).objects[0].present);
        rejects(f.h->dismiss_created(f.target, created), R::Error::MissingObject);
    });
    test("creation owner loss does not grant replacement control or extend absolute lifetime", [] {
        Scene f;
        f.cast("create stone-wall at G0 point 300 mm 0 mm for 2000 ms owned by this actor");
        ok(f.h->remove_actor(f.target));
        auto replacement = take(f.h->add_actor({{10, 0}, 50, 100, 0, 1000, true, true, false}));
        f.unchanged([&] { rejects(f.h->dismiss_created(replacement, {0, 1}), R::Error::Permission); });
        auto cp = take(f.h->checkpoint());
        ok(f.h->restore(cp));
        take(f.h->advance_to(200));
        check(!take(f.h->inspect_perception()).objects[0].present && f.h->snapshot().actors[replacement.slot].present);
    });
    test("created object shatter spends lifetime permanently and new generation survives", [] {
        Scene f;
        f.cast("create stone-wall at G0 point 300 mm 0 mm for 2000 ms owned by this actor");
        take(f.h->release(take(f.h->prepare(plan(subject_source("shatter object instant")), {f.caster, {}, {}, R::ObjectId{0, 1}}))));
        f.cast("create open-arch at G0 point 400 mm 0 mm for 3000 ms owned by this actor");
        auto m = take(f.h->inspect_mechanics());
        check(m.creations[0].object == R::ObjectId{0, 2} && m.creations[0].instance == 2);
        take(f.h->advance_to(200));
        check(take(f.h->inspect_perception()).objects[0].present);
        take(f.h->advance_to(300));
        check(!take(f.h->inspect_perception()).objects[0].present);
    });
    test("new object placement refuses occupied actors objects and full exact capacities", [] {
        for (auto clause :
             {"create stone-wall at G0 point 10 mm 0 mm for 2000 ms owned by this actor",
              "create stone-wall at G0 point -10 mm 0 mm for 2000 ms owned by this actor"}) {
            Scene f;
            f.unchanged([&] { rejects(f.h->prepare(plan(contact(clause)), {f.caster, f.target, {}}), R::Error::DestinationBlocked); });
        }
        for (std::size_t cap : {0u, 3u, 4u}) {
            R::HostLimits limits;
            limits.creations = cap;
            Scene f(limits);
            for (std::size_t i = 0; i < cap; ++i)
                f.cast("create stone-wall at G0 point " + std::to_string(300 + i * 100) + " mm 0 mm for 2000 ms owned by this actor");
            f.unchanged([&] {
                rejects(
                    f.h->prepare(
                        plan(contact("create open-arch at G0 point 900 mm 0 mm for 2000 ms owned by this actor")), {f.caster, f.target, {}}
                    ),
                    R::Error::MechanicLimit
                );
            });
            check(take(f.h->inspect_mechanics()).instances_issued == cap);
        }
        R::HostLimits l;
        l.objects = 0;
        auto h = take(R::ReferenceHost::create(game(), l));
        auto a = take(h->add_actor({{}, 100, 100, 100, 1000, true, true, true}));
        rejects(
            h->prepare(plan(contact("create open-arch at G0 point 900 mm 0 mm for 2000 ms owned by this actor")), {a, a, {}}),
            R::Error::ObjectLimit
        );
    });
    test("transmutation exact record slots overlap and generation expiry refusal", [] {
        for (std::size_t cap : {0u, 3u, 4u}) {
            R::HostLimits l;
            l.transmutations = cap;
            Scene f(l);
            std::array<R::ObjectId, 4> objects;
            for (std::size_t i = 0; i < 4; ++i)
                objects[i] = take(f.h->add_object({Appearance::StoneWall, false, {static_cast<std::int32_t>(300 + i * 100), 0}}));
            auto p = plan(subject_source("transmute object to open-arch for 2000 ms"));
            for (std::size_t i = 0; i < cap; ++i)
                take(f.h->release(take(f.h->prepare(p, {f.caster, {}, {}, objects[i]}))));
            if (cap < 4)
                f.unchanged([&] { rejects(f.h->prepare(p, {f.caster, {}, {}, objects[cap]}), R::Error::MechanicLimit); });
            else
                f.unchanged([&] { rejects(f.h->prepare(p, {f.caster, {}, {}, objects[0]}), R::Error::Overlap); });
        }
        R::HostLimits l;
        l.max_generation = 1;
        Scene f(l);
        f.cast("create open-arch at G0 point 300 mm 0 mm for 1000 ms owned by this actor");
        take(f.h->advance_to(100));
        f.unchanged([&] {
            rejects(
                f.h->prepare(
                    plan(contact("create open-arch at G0 point 400 mm 0 mm for 1000 ms owned by this actor")), {f.caster, f.target, {}}
                ),
                R::Error::GenerationLimit
            );
        });
    });
    test("material creation schema keys form duration and knowledge remain separate", [] {
        auto learner = take(L::Learner::create(2047));
        auto book = take(L::Spellbook::create());
        ok(book->add("material", "Material"));
        auto attempt = take(book->edit("material", subject_source("transmute object to open-arch for 2000 ms"), "", game(), learner));
        check(
            attempt->problem() && attempt->problem()->code == L::Error::UnknownKnowledge &&
            attempt->problem()->missing == rule_bit(Rule::CommonTransmute)
        );
        for (unsigned which = 0; which < 3; ++which) {
            Scene f;
            auto wall = take(f.h->add_object({Appearance::StoneWall, false, {300, 0}}));
            auto p = plan(subject_source("transmute object to open-arch for 2000 ms"));
            auto ticket = take(f.h->prepare(p, {f.caster, {}, {}, wall}));
            auto c = game();
            if (which == 0)
                c.operations[9].key = "unknown.material";
            if (which == 1)
                c.operations[9].appearances = 2;
            if (which == 2)
                c.operations[9].max_duration_ticks = 199;
            ok(f.h->replace_catalog(c));
            f.unchanged([&] { rejects(f.h->release(ticket), R::Error::SchemaMismatch); });
        }
        for (auto text :
             {subject_source("transmute object to water for 2000 ms"),
              contact("create stone-wall at G0 point 1.5 mm 0 mm for 2000 ms owned by this actor"),
              contact("create stone-wall at G0 point 300 mm 0 mm for 1 ms owned by this actor")})
            check(!compile(text, game()));
    });
    test("material and creation time horizons and identity exhaustion preserve complete state", [] {
        for (std::uint64_t cap : {199u, 200u}) {
            R::HostLimits l;
            l.max_tick = cap;
            Scene f(l);
            auto p = plan(contact("create stone-wall at G0 point 300 mm 0 mm for 2000 ms owned by this actor"));
            if (cap == 199)
                f.unchanged([&] { rejects(f.h->prepare(p, {f.caster, f.target, {}}), R::Error::TimeLimit); });
            else {
                take(f.h->release(take(f.h->prepare(p, {f.caster, f.target, {}}))));
                take(f.h->advance_to(200));
                check(!take(f.h->inspect_perception()).objects[0].present);
            }
        }
        R::HostLimits l;
        l.max_mechanic_instance = 0;
        Scene f(l);
        f.unchanged([&] {
            rejects(
                f.h->prepare(
                    plan(contact("create stone-wall at G0 point 300 mm 0 mm for 1000 ms owned by this actor")), {f.caster, f.target, {}}
                ),
                R::Error::MechanicLimit
            );
        });
    });
    test("material plus creation checkpoint retains actual plans and exact owned facts", [] {
        R::Checkpoint cp;
        R::ObjectId object;
        R::ActorId owner;
        std::weak_ptr<const Plan> created, material;
        {
            Scene f;
            owner = f.target;
            auto c = plan(contact("create stone-wall at G0 point 300 mm 0 mm for 3000 ms owned by this actor"));
            created = c;
            take(f.h->release(take(f.h->prepare(c, {f.caster, f.target, {}}))));
            object = {0, 1};
            auto t = plan(subject_source("transmute object to open-arch for 1000 ms"));
            material = t;
            take(f.h->release(take(f.h->prepare(t, {f.caster, {}, {}, object}))));
            cp = take(f.h->checkpoint());
        }
        check(!created.expired() && !material.expired());
        auto h = take(R::ReferenceHost::create(game()));
        ok(h->restore(cp));
        cp = {};
        take(h->advance_to(99));
        check(take(h->inspect_perception()).objects[0].data.passable);
        take(h->advance_to(100));
        check(!take(h->inspect_perception()).objects[0].data.passable && material.expired() && !created.expired());
        ok(h->dismiss_created(owner, object));
        check(created.expired());
        take(h->advance_to(300));
        check(!take(h->inspect_perception()).objects[0].present);
    });
    test("late delayed material failure rolls back earlier expiry and new scratch writes", [] {
        Scene f;
        auto a = take(f.h->add_object({Appearance::StoneWall, false, {300, 0}}));
        auto b = take(f.h->add_object({Appearance::StoneWall, false, {400, 0}}));
        auto immediate = plan(subject_source("transmute object to open-arch for 1000 ms"));
        take(f.h->release(take(f.h->prepare(immediate, {f.caster, {}, {}, a}))));
        auto delayed = plan(subject_source("transmute object to open-arch for 2000 ms", true));
        take(f.h->release(take(f.h->prepare(delayed, {f.caster, {}, {}, b}))));
        auto shatter = plan(subject_source("shatter object instant", true));
        take(f.h->release(take(f.h->prepare(shatter, {f.caster, {}, {}, b}))));
        take(f.h->release(take(f.h->prepare(shatter, {f.caster, {}, {}, b}))));
        f.unchanged([&] { rejects(f.h->advance_to(100), R::Error::MissingObject); });
        check(
            take(f.h->inspect_perception()).objects[a.slot].data.passable && f.h->snapshot().tick == 0 &&
            f.h->snapshot().actors[f.caster.slot].data.mana == 960
        );
        take(f.h->cancel(4));
        take(f.h->advance_to(100));
        check(!take(f.h->inspect_perception()).objects[a.slot].data.passable && !take(f.h->inspect_perception()).objects[b.slot].present);
    });
    test("restriction capacity4 versus3 refuses the whole delayed selection", [] {
        for (std::size_t cap : {3u, 4u}) {
            R::HostLimits l;
            l.restrictions = cap;
            Scene f(l);
            auto attacker = f.h->snapshot().actors[f.attacker.slot].data;
            attacker.eligible = false;
            ok(f.h->update_actor(f.attacker, attacker));
            for (int x : {20, 30, 40})
                take(f.h->add_actor({{x, 0}, 50, 100, 0, 1000, true, true, false}));
            auto p = plan(area("forbid attack for 2000 ms"));
            take(f.h->release(take(f.h->prepare(p, {f.caster, {}, R::Point{}}))));
            if (cap == 3) {
                f.unchanged([&] { rejects(f.h->advance_to(100), R::Error::MechanicLimit); });
                take(f.h->cancel(1));
                take(f.h->try_attack(f.attacker, f.target));
            } else {
                take(f.h->advance_to(100));
                check(take(f.h->inspect_mechanics()).restrictions[3].instance == 4);
            }
        }
    });
    test("all six source contracts keep exact token work and retained budgets", [] {
        const std::array<std::string, 6> sources = {
            contact("ward absorb 5 dp for 2000 ms"),
            contact("forbid attack for 2000 ms"),
            contact("relocate to G0 point -300 mm 400 mm instant"),
            subject_source("shatter object instant"),
            subject_source("transmute object to open-arch for 2000 ms"),
            contact("create stone-wall at G0 point 300 mm 0 mm for 2000 ms owned by this actor")
        };
        for (const auto& source : sources) {
            auto p = plan(source);
            auto m = p->metrics();
            std::cout << "M7 compiler primitive=" << static_cast<unsigned>(p->meaning().effects[0].primitive) << " tokens=" << m.tokens
                      << " work=" << m.work << " retained=" << m.retained_bytes << '\n';
            for (unsigned role = 0; role < 4; ++role)
                for (bool short_limit : {false, true}) {
                    Options o;
                    if (role == 0)
                        o.limits.source_bytes = source.size() - (short_limit ? 1 : 0);
                    if (role == 1)
                        o.limits.tokens = m.tokens - (short_limit ? 1 : 0);
                    if (role == 2)
                        o.limits.work = m.work - (short_limit ? 1 : 0);
                    if (role == 3)
                        o.limits.retained_bytes = m.retained_bytes - (short_limit ? 1 : 0);
                    auto r = compile(source, game(), o);
                    if (short_limit)
                        check(!r);
                    else
                        check(r && (*r)->meaning() == p->meaning());
                }
        }
    });
    test("new operations standalone bundles and engraving refusal remain complete", [] {
        const std::array<std::string, 6> sources = {
            contact("ward absorb 5 dp for 2000 ms"),
            contact("forbid attack for 2000 ms"),
            contact("relocate to G0 point 300 mm 0 mm instant"),
            subject_source("shatter object instant"),
            subject_source("transmute object to open-arch for 2000 ms"),
            contact("create stone-wall at G0 point 300 mm 0 mm for 2000 ms owned by this actor")
        };
        Scene f;
        for (const auto& source : sources) {
            auto p = plan(source);
            f.unchanged([&] { rejects(f.h->install(p, f.caster), R::Error::UnsupportedEntry); });
            auto mixed = source;
            mixed.insert(
                mixed.rfind("  }"), p->meaning().delivery == Delivery::Subject ? "on object bridge Common@1 { shatter object instant } ;\n"
                                                                               : "on hand bridge Common@1 { heal 1 hp instant } ;\n"
            );
            check(!compile(mixed, game()));
        }
    });
    test("all new knowledge concepts are compiler metadata without live authority", [] {
        const std::array<std::string, 6> sources = {
            contact("ward absorb 5 dp for 2000 ms"),
            contact("forbid attack for 2000 ms"),
            contact("relocate to G0 point 300 mm 0 mm instant"),
            subject_source("shatter object instant"),
            subject_source("transmute object to open-arch for 2000 ms"),
            contact("create stone-wall at G0 point 300 mm 0 mm for 2000 ms owned by this actor")
        };
        auto learner = take(L::Learner::create(2047));
        auto book = take(L::Spellbook::create());
        ok(book->add("new", "New operation"));
        Scene f;
        for (std::size_t i = 0; i < sources.size(); ++i) {
            auto attempt = take(book->edit("new", sources[i], "", game(), learner));
            check(
                attempt->problem() && attempt->problem()->code == L::Error::UnknownKnowledge &&
                attempt->problem()->missing == rule_bit(static_cast<Rule>(11 + i))
            );
            rejects(f.h->release({}), R::Error::InvalidTicket);
        }
    });
    test("checkpoint incompatibility and changed creation schema preserve live destination", [] {
        Scene f;
        f.cast("create stone-wall at G0 point 300 mm 0 mm for 2000 ms owned by this actor");
        auto cp = take(f.h->checkpoint());
        Scene target;
        auto ticket = take(target.h->prepare(plan(contact("ward absorb 5 dp for 2000 ms")), {target.caster, target.target, {}}));
        target.unchanged([&] { rejects(target.h->restore({}), R::Error::InvalidCheckpoint); });
        take(target.h->release(ticket));
        auto c = game();
        c.operations[10].max_duration_ticks = 199;
        ok(f.h->replace_catalog(c));
        f.unchanged([&] { rejects(f.h->checkpoint(), R::Error::SchemaMismatch); });
        auto foreign = take(R::ReferenceHost::create(c));
        auto before = foreign->snapshot();
        rejects(foreign->restore(cp), R::Error::IncompatibleCheckpoint);
        check(foreign->snapshot() == before);
    });
    test("creation live key primitive and form changes cannot mint a physical object", [] {
        for (unsigned which = 0; which < 4; ++which) {
            Scene f;
            auto p = plan(contact("create stone-wall at G0 point 300 mm 0 mm for 2000 ms owned by this actor"));
            auto ticket = take(f.h->prepare(p, {f.caster, f.target, {}}));
            auto c = game();
            if (which == 0)
                c.operations[10].key = "unknown.create";
            if (which == 1)
                c.operations[10].appearances = 1;
            if (which == 2)
                c.operations[10].max_duration_ticks = 199;
            if (which == 3)
                std::swap(c.operations[10].primitive, c.operations[9].primitive);
            ok(f.h->replace_catalog(c));
            f.unchanged([&] { rejects(f.h->release(ticket), R::Error::SchemaMismatch); });
            ok(f.h->replace_catalog(game()));
            f.cast("create stone-wall at G0 point 300 mm 0 mm for 2000 ms owned by this actor");
            check(take(f.h->inspect_perception()).objects[0].id == R::ObjectId{0, 1});
        }
    });
    test("live coordinate schema tightens payload positions independently of geometry", [] {
        for (bool creation : {false, true}) {
            Scene f;
            auto p = plan(contact(
                creation ? "create stone-wall at G0 point 300 mm 0 mm for 2000 ms owned by this actor"
                         : "relocate to G0 point 300 mm 0 mm instant"
            ));
            auto ticket = take(f.h->prepare(p, {f.caster, f.target, {}}));
            auto c = game();
            c.operations[creation ? 10 : 7].max_coordinate_mm = 299;
            ok(f.h->replace_catalog(c));
            f.unchanged([&] { rejects(f.h->release(ticket), R::Error::SchemaMismatch); });
            check(!compile(p->source(), c));
            c.operations[creation ? 10 : 7].max_coordinate_mm = 300;
            ok(f.h->replace_catalog(c));
            take(f.h->release(take(f.h->prepare(p, {f.caster, f.target, {}}))));
        }
        auto invalid = game();
        invalid.operations[7].max_coordinate_mm = 1'000'001;
        rejects(R::ReferenceHost::create(invalid), R::Error::InvalidCatalog);
    });
    test("each selected capability has exact complete-transition work and one-short refusal", [] {
        const std::array<std::string, 6> sources = {
            area("ward absorb 5 dp for 2000 ms"),
            area("forbid attack for 2000 ms"),
            contact("relocate to G0 point 300 mm 0 mm instant"),
            subject_source("shatter object instant", true),
            subject_source("transmute object to open-arch for 2000 ms", true),
            contact("create stone-wall at G0 point 300 mm 0 mm for 2000 ms owned by this actor")
        };
        for (const auto& source : sources) {
            auto p = plan(source);
            const bool object = p->meaning().delivery == Delivery::Subject;
            const bool delayed = p->meaning().onset_ticks != 0;
            auto request = [&](Scene& f) {
                return R::Request{
                    f.caster, p->meaning().delivery == Delivery::Contact ? std::optional<R::ActorId>(f.target) : std::nullopt,
                    p->meaning().delivery == Delivery::Burst ? std::optional<R::Point>(R::Point{}) : std::nullopt,
                    object ? std::optional<R::ObjectId>(R::ObjectId{0, 1}) : std::nullopt
                };
            };
            Scene measure;
            if (object)
                take(measure.h->add_object({Appearance::StoneWall, false, {300, 0}}));
            auto report = take(measure.h->release(take(measure.h->prepare(p, request(measure)))));
            if (delayed)
                report = take(measure.h->advance_to(100));
            const auto work = report->usage.work;
            std::cout << "M7 transition primitive=" << static_cast<unsigned>(p->meaning().effects[0].primitive) << " work=" << work
                      << " selector=" << report->usage.selector_work << " targets=" << report->usage.selected_targets << '\n';
            for (bool short_limit : {false, true}) {
                R::HostLimits l;
                l.work = work - (short_limit ? 1 : 0);
                Scene f(l);
                if (object)
                    take(f.h->add_object({Appearance::StoneWall, false, {300, 0}}));
                if (delayed) {
                    take(f.h->release(take(f.h->prepare(p, request(f)))));
                    if (short_limit)
                        f.unchanged([&] { rejects(f.h->advance_to(100), R::Error::WorkLimit); });
                    else
                        check(take(f.h->advance_to(100))->usage.work == work);
                } else if (short_limit)
                    f.unchanged([&] { rejects(f.h->prepare(p, request(f)), R::Error::WorkLimit); });
                else
                    check(take(f.h->release(take(f.h->prepare(p, request(f)))))->usage.work == work);
            }
        }
    });
    test("one owned checkpoint coherently resumes all new domains and old item witness slow state", [] {
        R::Checkpoint cp;
        R::ActorId caster, target, attacker;
        R::ObjectId created, other;
        R::ItemId item;
        R::TouchEvent event;
        R::Ticket old;
        R::AttackObserved old_hit;
        {
            Scene f;
            caster = f.caster;
            target = f.target;
            attacker = f.attacker;
            f.cast("ward absorb 5 dp for 2000 ms");
            old_hit = take(f.h->try_attack(attacker, target));
            f.cast("forbid attack for 2000 ms", attacker);
            f.cast("create stone-wall at G0 point 300 mm 0 mm for 4000 ms owned by this actor");
            created = {0, 1};
            take(f.h->release(
                take(f.h->prepare(plan(subject_source("transmute object to open-arch for 1000 ms")), {caster, {}, {}, created}))
            ));
            auto rune = plan(
                "engraved item-touch charges 1 release via item-event after 0 ms { bind toucher = willing event toucher at activation ; together ordered { on toucher bridge Common@1 { heal 12 hp instant } ; } }"
            );
            item = take(f.h->install(rune, caster));
            event = take(f.h->admit_touch(1, item, target));
            other = take(f.h->add_object({Appearance::OpenArch, true, {400, 0}}));
            auto d = plan(
                "spoken release via burst after 0 ms { bind audience = eligible actors in G0 disk 250 mm around aim captured at release queried at activation inclusive ; together ordered { on audience bridge Common@1 { show subject captured at release rechecked at activation as stone-wall for 2000 ms to this audience frozen at activation } ; } }"
            );
            take(f.h->release(take(f.h->prepare(d, {caster, {}, R::Point{}, created}))));
            auto sensing = plan(
                "spoken release via burst after 0 ms { bind observer = eligible actors in G0 disk 1 mm around aim captured at release queried at activation inclusive ; together ordered { on observer bridge Common@1 { discern subject captured at release rechecked at activation for this audience instant } ; } }"
            );
            take(f.h->release(take(f.h->prepare(sensing, {caster, {}, R::Point{10, 0}, created}))));
            old = take(f.h->prepare(plan(subject_source("shatter object instant", true)), {caster, {}, {}, other}));
            take(f.h->release(old));
            f.cast("multiply speed by 750 permille for 3000 ms");
            check(
                f.h->snapshot().actors[caster.slot].data.mana == 910 && f.h->snapshot().actors[target.slot].data.health == 50 &&
                take(f.h->inspect_mechanics()).wards[0].remaining_dp == 2 &&
                take(f.h->knowledge(target)).witnesses[0].observed_form == Appearance::OpenArch
            );
            cp = take(f.h->checkpoint());
        }
        auto h = take(R::ReferenceHost::create(game()));
        ok(h->restore(cp));
        cp = {};
        rejects(h->release(old), R::Error::ForeignTicket);
        rejects(h->touch(event), R::Error::ForeignEvent);
        take(h->touch(take(h->resume_touch(1))));
        check(h->snapshot().actors[target.slot].data.health == 62);
        take(h->advance_to(100));
        check(
            !take(h->inspect_perception()).objects[other.slot].present &&
            !take(h->inspect_perception()).objects[created.slot].data.passable &&
            take(h->knowledge(target)).witnesses[0].observed_form == Appearance::OpenArch
        );
        rejects(h->try_attack(attacker, target), R::Error::RestrictedIntent);
        take(h->advance_to(200));
        auto hit = take(h->try_attack(attacker, target));
        check(
            hit->damage == R::DamageResolution{3, 0, 3} && h->snapshot().actors[target.slot].data.health == 59 &&
            h->snapshot().actors[target.slot].slow == R::Slow{750, 300}
        );
        take(h->advance_to(300));
        check(h->snapshot().actors[target.slot].slow == R::Slow{});
        take(h->advance_to(400));
        check(
            !take(h->inspect_perception()).objects[created.slot].present && h->items().items[item.slot].charges == 0 &&
            h->items().ledger[0].consumed && take(h->knowledge(target)).count == 1 && old_hit->damage == R::DamageResolution{3, 3, 0}
        );
    });
    test("material creation state ceilings cannot expand and old source outputs remain owned", [] {
        for (unsigned field = 0; field < 3; ++field) {
            R::HostLimits l;
            if (field == 0)
                l.transmutations = 5;
            if (field == 1)
                l.creations = 5;
            if (field == 2)
                l.max_mechanic_instance = 1'000'001;
            rejects(R::ReferenceHost::create(game(), l), R::Error::InvalidLimits);
        }
        PlanHandle retained;
        std::string common;
        {
            auto source = contact("create open-arch at G0 point -300 mm 400 mm for 2000 ms owned by this actor");
            auto catalog = game();
            retained = take(compile(source, catalog));
            common = std::string(retained->common());
            source.clear();
            catalog.operations.clear();
        }
        check(retained->common() == common && std::get<Creation>(retained->meaning().effects[0].payload).position == WorldPoint{-300, 400});
    });
    test("existing five-field schema aggregates preserve appearance capability meaning", [] {
        Catalog restricted;
        restricted.operations = {{Primitive::Disguise, "game.disguise", 1'000'000, 8'640'000, 1}};
        const std::string disguise =
            "spoken release via burst after 0 ms { bind audience = eligible actors in G0 disk 250 mm around aim captured at release queried at activation inclusive ; together ordered { on audience bridge Common@1 { show subject captured at release rechecked at activation as stone-wall for 2000 ms to this audience frozen at activation } ; } }";
        rejects(compile(disguise, restricted), Code::UnsupportedOperation);
        auto p = plan(disguise);
        auto host = take(R::ReferenceHost::create(restricted));
        auto caster = take(host->add_actor({{}, 100, 100, 100, 1000, false, true, true}));
        take(host->add_actor({{10, 0}, 50, 100, 0, 1000, true, true, false}));
        auto object = take(host->add_object({}));
        rejects(host->prepare(p, {caster, {}, R::Point{}, object}), R::Error::SchemaMismatch);
    });
    test("old fire slow bundle refusal preserves new ward then later consumes only damage", [] {
        Scene f;
        f.cast("ward absorb 5 dp for 3000 ms");
        f.cast("multiply speed by 750 permille for 2000 ms");
        auto text = replace(area("fire damage 4 dp instant"), "after 1000 ms", "after 0 ms");
        text.insert(text.rfind("  }"), "on near bridge Common@1 { multiply speed by 600 permille for 2000 ms } ;\n");
        auto p = plan(text);
        f.unchanged([&] { rejects(f.h->prepare(p, {f.caster, {}, R::Point{}}), R::Error::ActiveSlow); });
        check(take(f.h->inspect_mechanics()).wards[0].remaining_dp == 5 && f.h->snapshot().actors[f.caster.slot].data.mana == 980);
        take(f.h->advance_to(200));
        take(f.h->release(take(f.h->prepare(p, {f.caster, {}, R::Point{}}))));
        check(
            take(f.h->inspect_mechanics()).wards[0].remaining_dp == 1 && f.h->snapshot().actors[f.target.slot].data.health == 50 &&
            f.h->snapshot().actors[f.attacker.slot].data.health == 96 && f.h->snapshot().actors[f.target.slot].slow == R::Slow{600, 400} &&
            f.h->snapshot().actors[f.caster.slot].data.mana == 960
        );
    });
    std::cout << passed << " M7 mechanics cases passed; " << failed << " failed.\n";
    return failed ? 1 : 0;
}
