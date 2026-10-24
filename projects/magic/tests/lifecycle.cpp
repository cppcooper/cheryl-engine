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
    void check(bool good) {
        if (!good)
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
        check(!v);
        check(v.error().code == c);
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
        c.operations = {{Primitive::Heal, "heal"},           {Primitive::Fire, "fire"},         {Primitive::Slow, "slow"},
                        {Primitive::Disguise, "disguise"},   {Primitive::Discern, "discern"},   {Primitive::Ward, "ward"},
                        {Primitive::Restrict, "restrict"},   {Primitive::Relocate, "relocate"}, {Primitive::Shatter, "shatter"},
                        {Primitive::Transmute, "transmute"}, {Primitive::Create, "create"},     {Primitive::Animate, "animate"},
                        {Primitive::Dispel, "dispel"}};
        return c;
    }
    std::string bound(std::string_view delivery, std::string_view role, std::string_view clause) {
        return "spoken release via " + std::string(delivery) + " after 0 ms { bind mark = " + std::string(role) +
               " captured at release ; together ordered { on mark bridge Common@1 { " + std::string(clause) + " } ; } }";
    }
    std::string animate(std::string_view duration = "2000 ms") {
        return bound("remains", "remains", "animate bound remains for " + std::string(duration) + " owned by caster");
    }
    std::string dispel(std::string_view kind = "attack-restriction") {
        return bound("influence", "influence", "dispel " + std::string(kind) + " bound influence instant");
    }
    std::string contact(std::string_view clause) {
        return "spoken release via contact after 0 ms { bind hand = willing touched actor at release ; together ordered { on hand bridge Common@1 { " +
               std::string(clause) + " } ; } }";
    }
    std::string area(std::string_view clause) {
        return "spoken release via burst after 0 ms { bind near = eligible actors in G0 disk 250 mm around aim captured at release queried at activation inclusive ; together ordered { on near bridge Common@1 { " +
               std::string(clause) + " } ; } }";
    }
    std::string replace(std::string s, std::string_view from, std::string_view to) {
        auto i = s.find(from);
        check(i != s.npos);
        s.replace(i, from.size(), to);
        return s;
    }
    PlanHandle plan(std::string source) {
        return take(compile(source, game()));
    }
    struct Fixture {
        std::unique_ptr<R::ReferenceHost> host;
        R::ActorId caster, target;
        R::RemainsId remains;
        Fixture(R::HostLimits limits = {}, std::uint32_t mana = 100) {
            host = take(R::ReferenceHost::create(game(), limits));
            caster = take(host->add_actor({{0, 0}, 100, 100, mana, 1000, false, true, true}));
            target = take(host->add_actor({{20, 0}, 50, 100, 0, 1000, true, true, false}));
            if (limits.remains)
                remains = take(host->add_remains({10, 0}));
        }
        R::Request animation() const { return {caster, {}, {}, {}, remains}; }
        R::ActorId spawn() {
            cast(plan(animate()), animation());
            return take(host->life()).servants[0].actor;
        }
        R::Observed cast(PlanHandle p, R::Request request) { return take(host->release(take(host->prepare(p, request)))); }
        template <class F> void unchanged(F run) {
            auto w = host->snapshot();
            auto l = take(host->life());
            auto m = take(host->inspect_mechanics());
            auto p = take(host->inspect_perception());
            auto i = host->items();
            run();
            check(
                w == host->snapshot() && l == take(host->life()) && m == take(host->inspect_mechanics()) &&
                p == take(host->inspect_perception()) && i == host->items()
            );
        }
        R::InfluenceRef restrict_actor(R::ActorId who) {
            cast(plan(contact("forbid attack for 3000 ms")), {caster, who, {}});
            auto m = take(host->inspect_mechanics());
            for (auto& r : m.restrictions)
                if (r.instance && r.actor == who)
                    return take(host->bind_restriction(r.instance, who));
            throw std::runtime_error("no restriction");
        }
    };
}
int main() {
    test("literal typed animation and exact Common roundtrip", [] {
        auto p = plan(animate());
        auto& m = p->meaning();
        check(
            m.delivery == Delivery::Remains && m.onset_ticks == 0 && m.bindings.size() == 1 &&
            m.bindings[0].query == Query::CapturedRemains && m.bindings[0].query_time == BindingTime::Release
        );
        check(m.effects.size() == 1 && m.effects[0] == Effect{Primitive::Animate, "animate", 0, 0, 0, Animation{200}});
        check(p->requirements() == (rule_bit(Rule::TargetBridge) | rule_bit(Rule::CommonAnimate)));
        check(take(compile(p->common(), game()))->meaning() == m);
    });
    test("literal two typed dispel kinds and exact Common", [] {
        for (auto kind : {InfluenceKind::Disguise, InfluenceKind::AttackRestriction}) {
            auto p = plan(dispel(kind == InfluenceKind::Disguise ? "disguise" : "attack-restriction"));
            check(
                p->meaning().delivery == Delivery::Influence &&
                p->meaning().effects[0] == Effect{Primitive::Dispel, "dispel", 0, 0, 0, Dispelling{kind}}
            );
            check(
                p->requirements() == (rule_bit(Rule::TargetBridge) | rule_bit(Rule::CommonDispel)) &&
                take(compile(p->common(), game()))->meaning() == p->meaning()
            );
        }
    });
    test("duration independent edit and owned diagnostic", [] {
        check(std::get<Animation>(plan(animate("3000 ms"))->meaning().effects[0].payload).duration_ticks == 300);
        Diagnostic d;
        {
            auto source = animate("3 dp");
            auto p = compile(source, game());
            check(!p);
            d = p.error();
        }
        check(d.code == Code::WrongDimension && !d.excerpt.empty() && d.span.length);
    });
    test("negative inexact zero huge and wrong duration refuse", [] {
        for (auto duration : {"-10 ms", "1 ms", "0 ms", "99999999999999999999999999999999999 ms", "2 mm"})
            check(!compile(animate(duration), game()));
    });
    test("wrong roles unknown kinds delayed bundles engraved refuse", [] {
        check(!compile(bound("influence", "influence", "animate bound remains for 2000 ms owned by caster"), game()));
        check(!compile(dispel("slow"), game()));
        check(!compile(replace(animate(), "after 0 ms", "after 1000 ms"), game()));
        auto source = animate();
        source = replace(source, "} ; } }", "} ; on mark bridge Common@1 { animate bound remains for 2000 ms owned by caster } ; } }");
        check(!compile(source, game()));
        check(!compile(replace(animate(), "spoken release", "engraved item-touch charges 1 release"), game()));
    });
    test("finite thirteen operation catalog validates duplicate and unknown", [] {
        auto c = game();
        check(bool(R::ReferenceHost::create(c)));
        c.operations.push_back(c.operations.back());
        refuses(R::ReferenceHost::create(c), R::Error::InvalidCatalog);
        c = game();
        c.operations.back().primitive = static_cast<Primitive>(14);
        refuses(R::ReferenceHost::create(c), R::Error::InvalidCatalog);
        check(!compile(animate(), c));
    });
    test("pure preview then atomic paid spawn exact literal state", [] {
        Fixture f;
        auto before = f.host->snapshot();
        auto preview = take(f.host->preview(plan(animate()), f.animation()));
        check(
            preview.mana_cost == 10 && preview.due_tick == 0 && preview.usage.selected_targets == 1 && preview.usage.selector_work == 20 &&
            f.host->snapshot() == before && !take(f.host->life()).remains[0].spent
        );
        auto report = f.cast(plan(animate()), f.animation());
        auto s = take(f.host->life());
        auto id = s.servants[0].actor;
        check(
            id == R::ActorId{2, 1} && s.remains[0].spent && s.servants[0].instance == 1 && s.servants[0].action == 1 &&
            s.servants[0].owner == f.caster && s.servants[0].onset_tick == 0 && s.servants[0].until_tick == 200
        );
        check(
            f.host->snapshot().actors[2].data == R::ActorSpec{{10, 0}, 20, 20, 0, 1000, true, true, false} &&
            f.host->snapshot().actors[0].data.mana == 90
        );
        take(f.host->advance_to(199));
        check(f.host->snapshot().actors[2].present);
        take(f.host->advance_to(200));
        check(!f.host->snapshot().actors[2].present && take(f.host->life()).remains[0].spent && report->after.actors[2].present);
    });
    test("exact resources and one-short preserves claim then valid action", [] {
        Fixture exact({}, 10);
        exact.spawn();
        check(exact.host->snapshot().actors[0].data.mana == 0);
        Fixture shorted({}, 9);
        shorted.unchanged([&] { refuses(shorted.host->prepare(plan(animate()), shorted.animation()), R::Error::Resources); });
        auto c = shorted.host->snapshot().actors[0].data;
        c.mana = 10;
        ok(shorted.host->update_actor(shorted.caster, c));
        shorted.spawn();
    });
    test("normal tickets default foreign repeated and stale refuse", [] {
        Fixture f, g;
        f.unchanged([&] { refuses(f.host->release({}), R::Error::InvalidTicket); });
        auto ticket = take(f.host->prepare(plan(animate()), f.animation()));
        refuses(g.host->release(ticket), R::Error::ForeignTicket);
        take(f.host->release(ticket));
        f.unchanged([&] { refuses(f.host->release(ticket), R::Error::ConsumedTicket); });
        auto r = take(g.host->add_remains({-10, 0}));
        auto req = g.animation();
        req.remains = r;
        auto stale = take(g.host->prepare(plan(animate()), req));
        take(g.host->advance_to(1));
        g.unchanged([&] { refuses(g.host->release(stale), R::Error::StaleTicket); });
        g.cast(plan(animate()), req);
    });
    test("live permission symbolic key primitive duration and coordinate caps", [] {
        for (unsigned mode = 0; mode < 5; ++mode) {
            Fixture f;
            auto p = plan(animate());
            auto c = game();
            if (mode == 0)
                c.operations[11].key = "changed";
            if (mode == 1)
                c.operations[11].primitive = Primitive::Heal;
            if (mode == 2)
                c.operations[11].max_duration_ticks = 199;
            if (mode == 3)
                c.operations[11].max_coordinate_mm = 9;
            if (mode == 4)
                c.spoken = false;
            if (mode == 1)
                c.operations.erase(c.operations.begin());
            ok(f.host->replace_catalog(c));
            f.unchanged([&] { refuses(f.host->prepare(p, f.animation()), R::Error::SchemaMismatch); });
            ok(f.host->replace_catalog(game()));
            f.spawn();
        }
        Fixture f;
        auto c = f.host->snapshot().actors[0].data;
        c.can_cast = false;
        ok(f.host->update_actor(f.caster, c));
        f.unchanged([&] { refuses(f.host->prepare(plan(animate()), f.animation()), R::Error::Permission); });
    });
    test("actor capacity failure leaves remains resources and later spawn", [] {
        R::HostLimits l;
        l.actors = 2;
        Fixture f(l);
        f.unchanged([&] { refuses(f.host->prepare(plan(animate()), f.animation()), R::Error::ActorLimit); });
        ok(f.host->remove_actor(f.target));
        auto id = f.spawn();
        check(id == R::ActorId{1, 2});
    });
    test("remains capacity zero exact one generation removal and reuse", [] {
        R::HostLimits l;
        l.remains = 0;
        Fixture zero(l);
        refuses(zero.host->add_remains({10, 0}), R::Error::RemainsLimit);
        l.remains = 1;
        Fixture f(l);
        f.unchanged([&] { refuses(f.host->add_remains({-10, 0}), R::Error::RemainsLimit); });
        ok(f.host->remove_remains(f.remains));
        auto newer = take(f.host->add_remains({10, 0}));
        check(newer == R::RemainsId{0, 2});
        refuses(f.host->prepare(plan(animate()), f.animation()), R::Error::MissingRemains);
        f.remains = newer;
        f.spawn();
    });
    test("remains and actor generation horizons refuse without wrapping", [] {
        R::HostLimits l;
        l.max_generation = 1;
        Fixture f(l);
        ok(f.host->remove_remains(f.remains));
        f.unchanged([&] { refuses(f.host->add_remains({10, 0}), R::Error::GenerationLimit); });
        Fixture g(l);
        auto servant = g.spawn();
        ok(g.host->remove_actor(servant));
        auto second = take(g.host->add_remains({-10, 0}));
        auto req = g.animation();
        req.remains = second;
        g.unchanged([&] { refuses(g.host->prepare(plan(animate()), req), R::Error::GenerationLimit); });
    });
    test("servant downward zero and one record exhaustion", [] {
        R::HostLimits l;
        l.servants = 0;
        Fixture zero(l);
        zero.unchanged([&] { refuses(zero.host->prepare(plan(animate()), zero.animation()), R::Error::ServantLimit); });
        l.servants = 1;
        Fixture f(l);
        auto s = f.spawn();
        auto another = take(f.host->add_remains({-10, 0}));
        auto req = f.animation();
        req.remains = another;
        f.unchanged([&] { refuses(f.host->prepare(plan(animate()), req), R::Error::ServantLimit); });
        ok(f.host->dismiss_servant(take(f.host->command_authority(f.caster, s))));
        f.cast(plan(animate()), req);
        check(take(f.host->life()).instances_issued == 2 && take(f.host->life()).remains[0].spent);
    });
    test("instance zero and exhausted one preserve spent and new claims", [] {
        R::HostLimits l;
        l.max_servant_instance = 0;
        Fixture zero(l);
        refuses(zero.host->prepare(plan(animate()), zero.animation()), R::Error::InstanceLimit);
        l.max_servant_instance = 1;
        Fixture f(l);
        auto s = f.spawn();
        ok(f.host->dismiss_servant(take(f.host->command_authority(f.caster, s))));
        auto another = take(f.host->add_remains({-10, 0}));
        auto req = f.animation();
        req.remains = another;
        f.unchanged([&] { refuses(f.host->prepare(plan(animate()), req), R::Error::InstanceLimit); });
    });
    test("invalid placement contact and opaque object block atomic claim", [] {
        Fixture f;
        auto wall = take(f.host->add_object({Appearance::StoneWall, false, {10, 0}}));
        f.unchanged([&] { refuses(f.host->prepare(plan(animate()), f.animation()), R::Error::DestinationBlocked); });
        ok(f.host->remove_object(wall));
        f.spawn();
        Fixture g;
        auto a = take(g.host->add_actor({{10, 0}}));
        g.unchanged([&] { refuses(g.host->prepare(plan(animate()), g.animation()), R::Error::DestinationBlocked); });
        ok(g.host->remove_actor(a));
        g.spawn();
        Fixture h;
        auto far = take(h.host->add_remains({51, 0}));
        auto req = h.animation();
        req.remains = far;
        refuses(h.host->prepare(plan(animate()), req), R::Error::Contact);
    });
    test("time exact and one-short duration horizons", [] {
        R::HostLimits l;
        l.max_tick = 200;
        Fixture exact(l);
        exact.spawn();
        take(exact.host->advance_to(200));
        l.max_tick = 199;
        Fixture shorted(l);
        shorted.unchanged([&] { refuses(shorted.host->prepare(plan(animate()), shorted.animation()), R::Error::TimeLimit); });
        shorted.cast(plan(animate("1990 ms")), shorted.animation());
    });
    test("owner-bound command attacks actual health through ward", [] {
        Fixture f;
        auto s = f.spawn();
        auto command = take(f.host->command_authority(f.caster, s));
        f.cast(plan(contact("ward absorb 5 dp for 2000 ms")), {f.caster, f.target, {}});
        auto first = take(f.host->command_attack(command, f.target));
        auto second = take(f.host->command_attack(command, f.target));
        check(
            first->damage == R::DamageResolution{3, 3, 0} && second->damage == R::DamageResolution{3, 2, 1} &&
            f.host->snapshot().actors[f.target.slot].data.health == 49 && first->after.actors[f.target.slot].data.health == 50
        );
    });
    test("default foreign wrong owner and departed replacement owners refuse", [] {
        Fixture f, g;
        auto s = f.spawn();
        auto command = take(f.host->command_authority(f.caster, s));
        refuses(f.host->command_attack({}, f.target), R::Error::InvalidCapability);
        refuses(g.host->command_attack(command, g.target), R::Error::ForeignCapability);
        refuses(f.host->command_authority(f.target, s), R::Error::Permission);
        ok(f.host->remove_actor(f.caster));
        auto replacement = take(f.host->add_actor({{0, 0}, 100, 100, 100, 1000, false, true, true}));
        check(replacement == R::ActorId{0, 2});
        f.unchanged([&] { refuses(f.host->command_attack(command, f.target), R::Error::MissingActor); });
        refuses(f.host->command_authority(replacement, s), R::Error::Permission);
        take(f.host->advance_to(200));
        check(!f.host->snapshot().actors[s.slot].present);
    });
    test("owner health loss inert and servant death no automatic remains", [] {
        Fixture f;
        auto s = f.spawn();
        auto command = take(f.host->command_authority(f.caster, s));
        auto c = f.host->snapshot().actors[0].data;
        c.health = 0;
        ok(f.host->update_actor(f.caster, c));
        refuses(f.host->command_attack(command, f.target), R::Error::Permission);
        c.health = 100;
        ok(f.host->update_actor(f.caster, c));
        auto servant = f.host->snapshot().actors[s.slot].data;
        servant.health = 0;
        ok(f.host->update_actor(s, servant));
        refuses(f.host->command_attack(command, f.target), R::Error::Permission);
        check(take(f.host->life()).remains[0].spent && !take(f.host->life()).remains[1].present);
    });
    test("dismiss remove reuse and old absolute cleanup protect replacements", [] {
        Fixture f;
        auto s = f.spawn();
        auto command = take(f.host->command_authority(f.caster, s));
        ok(f.host->dismiss_servant(command));
        check(take(f.host->life()).remains[0].spent && !take(f.host->life()).servants[0].instance);
        auto replacement = take(f.host->add_actor({{10, 0}, 70}));
        check(replacement == R::ActorId{2, 2});
        take(f.host->advance_to(200));
        check(f.host->snapshot().actors[2].present && f.host->snapshot().actors[2].data.health == 70);
        refuses(f.host->dismiss_servant(command), R::Error::MissingActor);
        refuses(f.host->prepare(plan(animate()), f.animation()), R::Error::SpentRemains);
    });
    test("active remains cannot be silently removed or reclaimed", [] {
        Fixture f;
        f.spawn();
        f.unchanged([&] {
            refuses(f.host->remove_remains(f.remains), R::Error::ServantLimit);
            refuses(f.host->prepare(plan(animate()), f.animation()), R::Error::SpentRemains);
        });
    });
    test("exact dispel enables previously forbidden real servant attack", [] {
        Fixture f;
        auto s = f.spawn();
        auto command = take(f.host->command_authority(f.caster, s));
        auto ref = f.restrict_actor(s);
        f.unchanged([&] { refuses(f.host->command_attack(command, f.target), R::Error::RestrictedIntent); });
        f.cast(plan(dispel()), {f.caster, {}, {}, {}, {}, ref});
        auto hit = take(f.host->command_attack(command, f.target));
        check(
            hit->damage == R::DamageResolution{3, 0, 3} && f.host->snapshot().actors[f.target.slot].data.health == 47 &&
            f.host->snapshot().actors[0].data.mana == 70
        );
        f.unchanged([&] { refuses(f.host->prepare(plan(dispel()), {f.caster, {}, {}, {}, {}, ref}), R::Error::MissingInfluence); });
    });
    test("dispel default foreign wrong-kind expired refs preserve world", [] {
        Fixture f, g;
        auto ref = f.restrict_actor(f.target);
        refuses(f.host->prepare(plan(dispel()), {f.caster, {}, {}}), R::Error::InvalidCapability);
        refuses(g.host->prepare(plan(dispel()), {g.caster, {}, {}, {}, {}, ref}), R::Error::ForeignCapability);
        refuses(f.host->prepare(plan(dispel("disguise")), {f.caster, {}, {}, {}, {}, ref}), R::Error::UnsupportedBinding);
        take(f.host->advance_to(300));
        f.unchanged([&] { refuses(f.host->prepare(plan(dispel()), {f.caster, {}, {}, {}, {}, ref}), R::Error::MissingInfluence); });
    });
    test("old ref cannot retarget new restriction or replaced actor", [] {
        Fixture f;
        auto old = f.restrict_actor(f.target);
        f.cast(plan(dispel()), {f.caster, {}, {}, {}, {}, old});
        auto fresh = f.restrict_actor(f.target);
        f.unchanged([&] { refuses(f.host->prepare(plan(dispel()), {f.caster, {}, {}, {}, {}, old}), R::Error::MissingInfluence); });
        f.cast(plan(dispel()), {f.caster, {}, {}, {}, {}, fresh});
        auto departed = f.restrict_actor(f.target);
        ok(f.host->remove_actor(f.target));
        auto replacement = take(f.host->add_actor({{20, 0}, 80}));
        check(replacement == R::ActorId{1, 2});
        f.unchanged([&] { refuses(f.host->prepare(plan(dispel()), {f.caster, {}, {}, {}, {}, departed}), R::Error::MissingInfluence); });
    });
    test("dispel current permission key primitive exact cost and no second debit", [] {
        Fixture f;
        auto ref = f.restrict_actor(f.target);
        auto p = plan(dispel());
        auto c = game();
        c.operations.back().key = "other";
        ok(f.host->replace_catalog(c));
        f.unchanged([&] { refuses(f.host->prepare(p, {f.caster, {}, {}, {}, {}, ref}), R::Error::SchemaMismatch); });
        ok(f.host->replace_catalog(game()));
        auto caster = f.host->snapshot().actors[0].data;
        caster.mana = 9;
        ok(f.host->update_actor(f.caster, caster));
        refuses(f.host->prepare(p, {f.caster, {}, {}, {}, {}, ref}), R::Error::Resources);
        caster.mana = 10;
        ok(f.host->update_actor(f.caster, caster));
        f.cast(p, {f.caster, {}, {}, {}, {}, ref});
        check(f.host->snapshot().actors[0].data.mana == 0);
        f.unchanged([&] { refuses(f.host->prepare(p, {f.caster, {}, {}, {}, {}, ref}), R::Error::MissingInfluence); });
    });
    test("selective disguise dispel preserves facts witnesses other instance", [] {
        Fixture f;
        auto observer = take(f.host->add_actor({{500, 0}, 60}));
        auto subject = take(f.host->add_object({}));
        auto show = plan(
            area("show subject captured at release rechecked at activation as stone-wall for 2000 ms to this audience frozen at activation")
        );
        f.cast(show, {f.caster, {}, R::Point{0, 0}, subject});
        f.cast(show, {f.caster, {}, R::Point{500, 0}, subject});
        auto old = take(f.host->perceive(f.target, subject));
        auto ref = take(f.host->bind_disguise(1, subject));
        f.cast(
            plan(area("discern subject captured at release rechecked at activation for this audience instant")),
            {f.caster, {}, R::Point{0, 0}, subject}
        );
        auto history = take(f.host->knowledge(f.target));
        check(history.count == 1 && history.witnesses[0].instance == 1 && history.witnesses[0].observed_passable);
        f.cast(plan(dispel("disguise")), {f.caster, {}, {}, {}, {}, ref});
        auto truth = take(f.host->inspect_perception()).objects[0];
        check(
            truth.data.form == Appearance::OpenArch && truth.data.passable &&
            take(f.host->perceive(f.target, subject)).form == Appearance::OpenArch &&
            take(f.host->perceive(observer, subject)).form == Appearance::StoneWall && take(f.host->knowledge(f.target)).count == 1 &&
            old.form == Appearance::StoneWall
        );
    });
    test("checkpoint owns active servant spent claim and influence after owner destruction", [] {
        R::Checkpoint cp;
        R::ActorId caster, target, servant;
        R::ServantCommand old_command;
        R::InfluenceRef old_ref;
        R::Ticket ticket;
        R::Observed old;
        {
            Fixture f;
            caster = f.caster;
            target = f.target;
            servant = f.spawn();
            old_command = take(f.host->command_authority(caster, servant));
            old_ref = f.restrict_actor(servant);
            old = f.cast(plan(contact("fire damage 4 dp instant")), {caster, target, {}});
            cp = take(f.host->checkpoint());
            ticket = take(f.host->prepare(plan(dispel()), {caster, {}, {}, {}, {}, old_ref}));
        }
        auto host = take(R::ReferenceHost::create(game()));
        ok(host->restore(cp));
        check(
            take(host->life()).remains[0].spent && host->snapshot().actors[0].data.mana == 70 &&
            host->snapshot().actors[target.slot].data.health == 46
        );
        refuses(host->release(ticket), R::Error::ForeignTicket);
        refuses(host->command_attack(old_command, target), R::Error::ForeignCapability);
        refuses(host->prepare(plan(dispel()), {caster, {}, {}, {}, {}, old_ref}), R::Error::ForeignCapability);
        auto ref = take(host->bind_restriction(1, servant));
        take(host->release(take(host->prepare(plan(dispel()), {caster, {}, {}, {}, {}, ref}))));
        auto command = take(host->command_authority(caster, servant));
        take(host->command_attack(command, target));
        check(host->snapshot().actors[target.slot].data.health == 43 && old->after.actors[target.slot].data.health == 46);
        take(host->advance_to(200));
        check(!host->snapshot().actors[servant.slot].present && take(host->life()).remains[0].spent);
    });
    test("checkpoint schema refusal preserves destination and usable old output", [] {
        Fixture f;
        f.spawn();
        auto cp = take(f.host->checkpoint());
        auto old = f.host->snapshot();
        auto c = game();
        c.operations[11].max_duration_ticks = 199;
        ok(f.host->replace_catalog(c));
        f.unchanged([&] {
            refuses(f.host->restore(cp), R::Error::IncompatibleCheckpoint);
            refuses(f.host->checkpoint(), R::Error::SchemaMismatch);
        });
        ok(f.host->replace_catalog(game()));
        ok(f.host->restore(cp));
        check(f.host->snapshot() == old);
        take(f.host->advance_to(200));
    });
    test("book knowledge edit revisions and failed draft grant no authority", [] {
        auto book = take(L::Spellbook::create());
        ok(book->add("life", "Owned life"));
        auto learned = take(L::Learner::create(all_rules & ~rule_bit(Rule::CommonAnimate)));
        auto unknown = take(book->edit("life", animate(), "Recognized but not learned", game(), learned));
        check(unknown->problem() && unknown->problem()->code == L::Error::UnknownKnowledge && !unknown->accepted());
        learned = take(L::Learner::create(all_rules));
        auto accepted = take(book->edit("life", animate(), "Temporary owned servant", game(), learned))->accepted();
        check(accepted && accepted->revision() == 1);
        auto failed_edit = take(book->edit("life", animate("1 ms"), "Repair duration", game(), learned));
        check(failed_edit->problem() && failed_edit->accepted() == accepted);
        auto newer = take(book->edit("life", animate("3000 ms"), "Longer", game(), learned))->accepted();
        check(newer->revision() == 2 && std::get<Animation>(accepted->plan()->meaning().effects[0].payload).duration_ticks == 200);
        Fixture f;
        refuses(f.host->release({}), R::Error::InvalidTicket);
        f.cast(accepted->plan(), f.animation());
        check(take(f.host->life()).servants[0].until_tick == 200);
    });
    test("exact selector and selected-target boundaries", [] {
        R::HostLimits l;
        l.selector_work = 20;
        l.selected_targets = 1;
        Fixture exact(l);
        exact.spawn();
        l.selector_work = 19;
        Fixture less(l);
        less.unchanged([&] { refuses(less.host->prepare(plan(animate()), less.animation()), R::Error::SelectorLimit); });
        l.selector_work = 20;
        l.selected_targets = 0;
        Fixture none(l);
        refuses(none.host->prepare(plan(animate()), none.animation()), R::Error::TargetLimit);
    });
    test("exact measured release work and one-short preserve ticket", [] {
        Fixture measured;
        auto p = plan(animate());
        auto result = measured.cast(p, measured.animation());
        auto work = result->usage.work;
        std::cout << "MEASURE animate release work=" << work << '\n';
        R::HostLimits l;
        l.work = work;
        Fixture exact(l);
        exact.spawn();
        l.work = work - 1;
        Fixture shorted(l);
        const auto before = shorted.host->snapshot();
        const auto life = take(shorted.host->life());
        refuses(shorted.host->prepare(p, shorted.animation()), R::Error::WorkLimit);
        check(shorted.host->snapshot() == before && take(shorted.host->life()) == life);
    });
    test("four exact remains and servants retain distinct claims and generations", [] {
        Fixture f;
        std::array<R::RemainsId, 4> remains{
            f.remains, take(f.host->add_remains({-10, 0})), take(f.host->add_remains({0, 10})), take(f.host->add_remains({0, -10}))
        };
        for (std::size_t i = 0; i < 4; ++i) {
            auto req = f.animation();
            req.remains = remains[i];
            f.cast(plan(animate()), req);
            const auto state = take(f.host->life());
            check(
                state.servants[i].instance == i + 1 && state.servants[i].actor == R::ActorId{static_cast<std::uint32_t>(i + 2), 1} &&
                state.remains[i].spent
            );
        }
        check(take(f.host->life()).instances_issued == 4 && f.host->snapshot().actors[0].data.mana == 60);
        f.unchanged([&] { refuses(f.host->add_remains({-20, 0}), R::Error::RemainsLimit); });
        auto saved = take(f.host->life());
        take(f.host->advance_to(200));
        for (std::size_t i = 0; i < 4; ++i)
            check(
                !f.host->snapshot().actors[i + 2].present && take(f.host->life()).remains[i].spent && saved.servants[i].instance == i + 1
            );
    });
    test("sixteen actor exact capacity then one-short claim is preserved", [] {
        Fixture f;
        std::array<R::ActorId, 13> extras;
        for (std::size_t i = 0; i < 13; ++i)
            extras[i] = take(f.host->add_actor({{static_cast<std::int32_t>(1000 + i * 100), 0}}));
        auto last = f.spawn();
        check(last == R::ActorId{15, 1});
        auto second = take(f.host->add_remains({-10, 0}));
        auto req = f.animation();
        req.remains = second;
        f.unchanged([&] { refuses(f.host->prepare(plan(animate()), req), R::Error::ActorLimit); });
        ok(f.host->remove_actor(extras[0]));
        f.cast(plan(animate()), req);
        check(take(f.host->life()).servants[1].actor == R::ActorId{2, 2});
    });
    test("owned life projection exact300 work and one-short299", [] {
        R::HostLimits l;
        l.work = 300;
        Fixture exact(l);
        check(take(exact.host->life()).remains[0].id == R::RemainsId{0, 1});
        l.work = 299;
        Fixture shorted(l);
        auto before = shorted.host->snapshot();
        refuses(shorted.host->life(), R::Error::WorkLimit);
        check(shorted.host->snapshot() == before);
    });
    test("complete command exact326 work and one-short325 with actual wardless injury", [] {
        auto scenario = [](std::size_t work) {
            Catalog c;
            c.operations = {{Primitive::Animate, "a"}};
            R::HostLimits l;
            l.work = work;
            auto host = take(R::ReferenceHost::create(c, l));
            auto owner = take(host->add_actor({{0, 0}, 100, 100, 100, 1000, false, true, true}));
            auto target = take(host->add_actor({{20, 0}, 50}));
            auto source = replace(animate(), "mark", "r");
            source = replace(source, "on mark", "on r");
            auto p = take(compile(source, c));
            for (auto position : {R::Point{10, 0}, R::Point{-10, 0}, R::Point{0, 10}, R::Point{0, -10}}) {
                auto remains = take(host->add_remains(position));
                take(host->release(take(host->prepare(p, {owner, {}, {}, {}, remains}))));
            }
            auto servant = take(host->life()).servants[3].actor;
            auto command = take(host->command_authority(owner, servant));
            auto before = host->snapshot();
            auto result = host->command_attack(command, target);
            if (work == 326)
                check(
                    result && (*result)->damage == R::DamageResolution{3, 0, 3} && host->snapshot().actors[target.slot].data.health == 47
                );
            else {
                refuses(result, R::Error::WorkLimit);
                check(host->snapshot() == before);
                ok(host->dismiss_servant(command));
            }
        };
        scenario(326);
        scenario(325);
    });
    test("dispel exact1148 work and one-short1147 preserves instance payment", [] {
        for (auto work : {1148u, 1147u}) {
            R::HostLimits l;
            l.work = work;
            Fixture f(l);
            auto ref = f.restrict_actor(f.target);
            auto c = game();
            c.operations.back().key = std::string(64, 'd');
            ok(f.host->replace_catalog(c));
            auto p = take(compile(dispel(), c));
            R::Request request{f.caster, {}, {}, {}, {}, ref};
            if (work == 1148) {
                auto result = f.cast(p, request);
                check(result->usage.work == 1148 && f.host->snapshot().actors[0].data.mana == 80);
            } else
                f.unchanged([&] { refuses(f.host->prepare(p, request), R::Error::WorkLimit); });
        }
    });
    test("new host ceilings reject upward knobs and support downward zero", [] {
        R::HostLimits l;
        l.remains = 5;
        refuses(R::ReferenceHost::create(game(), l), R::Error::InvalidLimits);
        l = {};
        l.servants = 5;
        refuses(R::ReferenceHost::create(game(), l), R::Error::InvalidLimits);
        l = {};
        l.max_servant_instance = 1'000'001;
        refuses(R::ReferenceHost::create(game(), l), R::Error::InvalidLimits);
        l = {};
        l.remains = 0;
        l.servants = 0;
        l.max_servant_instance = 0;
        auto host = take(R::ReferenceHost::create(game(), l));
        auto cp = take(host->checkpoint());
        ok(host->restore(cp));
        check(take(host->life()).instances_issued == 0);
    });
    test("action ticket revision exhaustion preserves claims and unrelated valid actions", [] {
        for (unsigned mode = 0; mode < 3; ++mode) {
            R::HostLimits l;
            if (mode == 0)
                l.max_action = 0;
            if (mode == 1)
                l.max_ticket = 0;
            if (mode == 2)
                l.max_revision = 3;
            Fixture f(l);
            auto before = f.host->snapshot();
            auto life = take(f.host->life());
            if (mode == 1)
                refuses(f.host->prepare(plan(animate()), f.animation()), R::Error::TicketLimit);
            else {
                auto t = take(f.host->prepare(plan(animate()), f.animation()));
                refuses(f.host->release(t), mode == 0 ? R::Error::ActionLimit : R::Error::RevisionLimit);
                ok(f.host->discard(t));
            }
            check(f.host->snapshot() == before && take(f.host->life()) == life);
            if (mode != 2)
                check(take(f.host->try_attack(f.caster, f.target))->damage.health_lost == 3);
        }
    });
    test("new compiler source token storage work exact and one-short", [] {
        for (auto source : {animate(), dispel()}) {
            auto p = plan(source);
            Options exact;
            exact.limits.source_bytes = source.size();
            exact.limits.tokens = p->metrics().tokens;
            exact.limits.work = p->metrics().work;
            exact.limits.retained_bytes = p->metrics().retained_bytes;
            check(bool(compile(source, game(), exact)));
            auto less = exact;
            --less.limits.source_bytes;
            refuses(compile(source, game(), less), Code::SourceLimit);
            less = exact;
            --less.limits.tokens;
            refuses(compile(source, game(), less), Code::TokenLimit);
            less = exact;
            --less.limits.work;
            refuses(compile(source, game(), less), Code::WorkLimit);
            less = exact;
            --less.limits.retained_bytes;
            refuses(compile(source, game(), less), Code::StorageLimit);
        }
    });
    test("retained plan runtime bytes exact and one-short refusal", [] {
        auto p = plan(animate());
        R::HostLimits l;
        l.plan_bytes = p->metrics().retained_bytes;
        Fixture f(l);
        f.spawn();
        take(f.host->checkpoint());
        --l.plan_bytes;
        Fixture shorted(l);
        shorted.unchanged([&] { refuses(shorted.host->prepare(p, shorted.animation()), R::Error::PlanLimit); });
    });
    test("extraneous actor aim object remains and influence roles refuse", [] {
        Fixture f;
        auto req = f.animation();
        req.contact = f.target;
        refuses(f.host->prepare(plan(animate()), req), R::Error::UnsupportedBinding);
        req = f.animation();
        req.aim = R::Point{0, 0};
        refuses(f.host->prepare(plan(animate()), req), R::Error::UnsupportedBinding);
        auto ref = f.restrict_actor(f.target);
        req = f.animation();
        req.influence = ref;
        refuses(f.host->prepare(plan(animate()), req), R::Error::UnsupportedBinding);
        req = {f.caster, f.target, {}, {}, f.remains};
        refuses(f.host->prepare(plan(contact("heal 12 hp instant")), req), R::Error::UnsupportedBinding);
    });
    test("dispel live primitive mismatch permission and subject capability refuse", [] {
        Fixture f;
        auto ref = f.restrict_actor(f.target);
        auto p = plan(dispel());
        R::Request req{f.caster, {}, {}, {}, {}, ref};
        auto c = game();
        c.operations.back().primitive = Primitive::Heal;
        c.operations.erase(c.operations.begin());
        ok(f.host->replace_catalog(c));
        f.unchanged([&] { refuses(f.host->prepare(p, req), R::Error::SchemaMismatch); });
        c = game();
        c.subject = false;
        ok(f.host->replace_catalog(c));
        refuses(f.host->prepare(p, req), R::Error::SchemaMismatch);
        check(!compile(dispel(), c));
        ok(f.host->replace_catalog(game()));
        auto caster = f.host->snapshot().actors[0].data;
        caster.can_cast = false;
        ok(f.host->update_actor(f.caster, caster));
        refuses(f.host->prepare(p, req), R::Error::Permission);
        caster.can_cast = true;
        ok(f.host->update_actor(f.caster, caster));
        f.cast(p, req);
    });
    test("new unsupported combinations retain precise old capability boundaries", [] {
        for (auto source : {animate(), dispel()}) {
            check(!compile(replace(source, "after 0 ms", "after 10 ms"), game()));
            check(!compile(replace(source, "spoken release", "engraved item-touch charges 1 release"), game()));
            auto mixed = replace(source, "} ; } }", "} ; on mark bridge Common@1 { heal 12 hp instant } ; } }");
            check(!compile(mixed, game()));
        }
        Fixture f;
        refuses(f.host->install(plan(animate()), f.caster), R::Error::UnsupportedEntry);
        refuses(f.host->install(plan(dispel()), f.caster), R::Error::UnsupportedEntry);
    });
    test("disguise exact ref cannot target reused object or later instance", [] {
        Fixture f;
        auto object = take(f.host->add_object({}));
        auto show = plan(
            area("show subject captured at release rechecked at activation as stone-wall for 2000 ms to this audience frozen at activation")
        );
        f.cast(show, {f.caster, {}, R::Point{0, 0}, object});
        auto old = take(f.host->bind_disguise(1, object));
        ok(f.host->remove_object(object));
        auto replacement = take(f.host->add_object({}));
        check(replacement == R::ObjectId{0, 2});
        f.cast(show, {f.caster, {}, R::Point{0, 0}, replacement});
        auto current = take(f.host->bind_disguise(2, replacement));
        f.unchanged([&] {
            refuses(f.host->prepare(plan(dispel("disguise")), {f.caster, {}, {}, {}, {}, old}), R::Error::MissingInfluence);
        });
        f.cast(plan(dispel("disguise")), {f.caster, {}, {}, {}, {}, current});
        check(take(f.host->perceive(f.target, replacement)).form == Appearance::OpenArch);
    });
    test("disguise removal visibly changes unrevealed observer and retains dated knowledge", [] {
        Fixture f;
        auto b = take(f.host->add_actor({{30, 0}, 60}));
        auto object = take(f.host->add_object({}));
        f.cast(
            plan(area(
                "show subject captured at release rechecked at activation as stone-wall for 2000 ms to this audience frozen at activation"
            )),
            {f.caster, {}, R::Point{0, 0}, object}
        );
        auto old = take(f.host->perceive(b, object));
        auto discern = plan(replace(
            area("discern subject captured at release rechecked at activation for this audience instant"), "disk 250 mm", "disk 1 mm"
        ));
        f.cast(discern, {f.caster, {}, R::Point{20, 0}, object});
        check(
            take(f.host->knowledge(f.target)).count == 1 && take(f.host->knowledge(b)).count == 0 &&
            take(f.host->perceive(b, object)).form == Appearance::StoneWall
        );
        auto ref = take(f.host->bind_disguise(1, object));
        f.cast(plan(dispel("disguise")), {f.caster, {}, {}, {}, {}, ref});
        check(
            take(f.host->perceive(b, object)).form == Appearance::OpenArch &&
            take(f.host->knowledge(f.target)).witnesses[0].instance == 1 && take(f.host->inspect_perception()).objects[0].data.passable &&
            old.form == Appearance::StoneWall
        );
    });
    test("late failed advance rolls back servant cleanup slow expiry and paid queue", [] {
        Fixture f;
        auto servant = f.spawn();
        f.cast(plan(contact("multiply speed by 750 permille for 2000 ms")), {f.caster, f.target, {}});
        auto source = replace(area("fire damage 4 dp instant"), "after 0 ms", "after 2000 ms");
        f.cast(plan(source), {f.caster, {}, R::Point{1000, 0}});
        const auto life = take(f.host->life());
        const auto world = f.host->snapshot();
        f.unchanged([&] { refuses(f.host->advance_to(200), R::Error::EmptySelection); });
        check(
            f.host->snapshot() == world && take(f.host->life()) == life && f.host->snapshot().actors[servant.slot].present &&
            f.host->snapshot().actors[f.target.slot].slow == R::Slow{750, 200} && f.host->snapshot().actors[0].data.mana == 70
        );
        take(f.host->cancel(3));
        take(f.host->advance_to(200));
        check(
            !f.host->snapshot().actors[servant.slot].present && f.host->snapshot().actors[f.target.slot].slow == R::Slow{} &&
            take(f.host->life()).remains[0].spent && f.host->snapshot().actors[0].data.mana == 70
        );
    });
    test("checkpoint with departed owner restores inert servant and absolute expiry", [] {
        Fixture f;
        auto servant = f.spawn();
        ok(f.host->remove_actor(f.caster));
        auto cp = take(f.host->checkpoint());
        auto original = take(f.host->life());
        f.host.reset();
        auto host = take(R::ReferenceHost::create(game()));
        ok(host->restore(cp));
        refuses(host->command_authority(f.caster, servant), R::Error::MissingActor);
        take(host->advance_to(200));
        check(
            !host->snapshot().actors[servant.slot].present && take(host->life()).remains[0].spent && original.servants[0].actor == servant
        );
    });
    test("prepared animation rechecks current permission without consuming authority", [] {
        Fixture f;
        auto p = plan(animate());
        auto ticket = take(f.host->prepare(p, f.animation()));
        auto caster = f.host->snapshot().actors[0].data;
        caster.can_cast = false;
        ok(f.host->update_actor(f.caster, caster));
        f.unchanged([&] { refuses(f.host->release(ticket), R::Error::Permission); });
        ok(f.host->discard(ticket));
        caster.can_cast = true;
        ok(f.host->update_actor(f.caster, caster));
        f.spawn();
    });
    test("prepared animation rechecks replaced remains and preserves fresh claim", [] {
        Fixture f;
        auto ticket = take(f.host->prepare(plan(animate()), f.animation()));
        ok(f.host->remove_remains(f.remains));
        auto fresh = take(f.host->add_remains({10, 0}));
        f.unchanged([&] { refuses(f.host->release(ticket), R::Error::MissingRemains); });
        ok(f.host->discard(ticket));
        f.remains = fresh;
        f.spawn();
        check(take(f.host->life()).remains[0].id == R::RemainsId{0, 2});
    });
    test("prepared exact dispel rechecks expiry and cannot remove a later instance", [] {
        Fixture f;
        auto ref = f.restrict_actor(f.target);
        auto ticket = take(f.host->prepare(plan(dispel()), {f.caster, {}, {}, {}, {}, ref}));
        take(f.host->advance_to(300));
        f.unchanged([&] { refuses(f.host->release(ticket), R::Error::MissingInfluence); });
        ok(f.host->discard(ticket));
        auto later = f.restrict_actor(f.target);
        f.unchanged([&] { refuses(f.host->prepare(plan(dispel()), {f.caster, {}, {}, {}, {}, ref}), R::Error::MissingInfluence); });
        f.cast(plan(dispel()), {f.caster, {}, {}, {}, {}, later});
        check(f.host->snapshot().actors[0].data.mana == 70);
    });
    test("incompatible restore preserves current servant command and prior checkpoint", [] {
        Fixture source;
        source.spawn();
        auto cp = take(source.host->checkpoint());
        R::HostLimits l;
        l.servants = 3;
        Fixture destination(l);
        auto servant = destination.spawn();
        auto command = take(destination.host->command_authority(destination.caster, servant));
        destination.unchanged([&] { refuses(destination.host->restore(cp), R::Error::IncompatibleCheckpoint); });
        check(take(destination.host->command_attack(command, destination.target))->damage.health_lost == 3);
        auto restore = take(R::ReferenceHost::create(game()));
        ok(restore->restore(cp));
        check(
            restore->snapshot().actors[source.target.slot].data.health == 50 && take(restore->life()).servants[0].actor == R::ActorId{2, 1}
        );
    });
    std::cout << passed << " lifecycle cases passed; " << failed << " failed\n";
    return failed ? 1 : 0;
}
