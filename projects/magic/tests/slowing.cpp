#include <magic/reference-host.h>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace {
    using namespace Magic;
    namespace R = Magic::Reference;
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
        c.operations = {{Primitive::Heal, "heal"}, {Primitive::Fire, "fire"},         {Primitive::Slow, "slow"},
                        {Primitive::Ward, "ward"}, {Primitive::Restrict, "restrict"}, {Primitive::Dispel, "dispel"}};
        return c;
    }
    std::string contact(std::string_view clause) {
        return "spoken release via contact after 0 ms { bind hand = willing touched actor at release ; together ordered { on hand bridge Common@1 { " +
               std::string(clause) + " } ; } }";
    }
    std::string slow(std::uint32_t factor = 750, std::uint32_t ms = 2000) {
        return contact("multiply speed by " + std::to_string(factor) + " permille for " + std::to_string(ms) + " ms");
    }
    std::string dispel(std::string_view kind = "slow") {
        return "spoken release via influence after 0 ms { bind mark = influence captured at release ; together ordered { on mark bridge Common@1 { dispel " +
               std::string(kind) + " bound influence instant } ; } }";
    }
    std::string burst(std::uint32_t delay = 1000) {
        return "spoken release via burst after " + std::to_string(delay) +
               " ms { bind near = eligible actors in G0 disk 50 mm around aim captured at release queried at activation inclusive ; together ordered { on near bridge Common@1 { fire damage 4 dp instant } ; on near bridge Common@1 { multiply speed by 500 permille for 3000 ms } ; } }";
    }
    std::string item() {
        return "engraved item-touch charges 1 release via item-event after 0 ms { bind hand = willing event toucher at activation ; together ordered { on hand bridge Common@1 { heal 3 hp instant } ; on hand bridge Common@1 { multiply speed by 600 permille for 4000 ms } ; } }";
    }
    PlanHandle plan(std::string source) {
        return take(compile(source, game()));
    }
    struct Fixture {
        std::unique_ptr<R::ReferenceHost> host;
        R::ActorId a, b, target;
        Fixture(R::HostLimits limits = {}) {
            host = take(R::ReferenceHost::create(game(), limits));
            a = take(host->add_actor({{0, 0}, 100, 100, 1000, 1000, false, true, true}));
            b = take(host->add_actor({{10, 0}, 100, 100, 1000, 1000, false, true, true}));
            target = take(host->add_actor({{20, 0}, 50}));
        }
        R::Observed cast(PlanHandle p, R::Request r) { return take(host->release(take(host->prepare(p, r)))); }
        R::Observed add(std::uint32_t factor = 750, std::uint32_t ms = 2000) { return cast(plan(slow(factor, ms)), {a, target, {}}); }
        R::InfluenceRef ref(std::uint64_t instance = 1) { return take(host->bind_slow(instance, target)); }
        R::Request removal(R::InfluenceRef r) const { return {a, {}, {}, {}, {}, r}; }
        template <class F> void unchanged(F run) {
            auto w = host->snapshot();
            auto s = take(host->slows());
            auto m = take(host->inspect_mechanics());
            auto i = host->items();
            run();
            check(w == host->snapshot() && s == take(host->slows()) && m == take(host->inspect_mechanics()) && i == host->items());
        }
    };
}
int main() {
    test("typed Slow dispel exact Common and concept", [] {
        auto p = plan(dispel());
        check(p->meaning().effects[0] == Effect{Primitive::Dispel, "dispel", 0, 0, 0, Dispelling{InfluenceKind::Slow}});
        check(p->requirements() == (rule_bit(Rule::TargetBridge) | rule_bit(Rule::CommonDispel)));
        check(take(compile(p->common(), game()))->meaning() == p->meaning());
        check(p->common().find("dispel slow bound influence instant") != std::string::npos);
    });
    test("unsupported kind delay composition and entry refuse at compiler", [] {
        check(!compile(dispel("ward"), game()));
        auto s = dispel();
        s.replace(s.find("after 0"), 7, "after 1");
        check(!compile(s, game()));
        s = dispel();
        s.replace(0, 14, "engraved item-touch charges 1 release");
        check(!compile(s, game()));
        s = dispel();
        s.insert(s.rfind("} }"), "on mark bridge Common@1 { fire damage 4 dp instant } ; ");
        check(!compile(s, game()));
    });
    test("distinct casts retain exact caster action effect and owned inspection", [] {
        Fixture f;
        f.add();
        auto old = take(f.host->slows());
        f.cast(plan(slow(500, 3000)), {f.b, f.target, {}});
        auto rows = take(f.host->slows());
        check(
            rows.instances_issued == 2 && rows.influences[0].source == R::SlowSource{f.a, 1} &&
            rows.influences[1].source == R::SlowSource{f.b, 2}
        );
        check(rows.influences[0].effect_index == 0 && rows.influences[0].plan_bytes == plan(slow())->metrics().retained_bytes);
        check(f.host->snapshot().actors[2].slow == R::Slow{500, 200} && old.influences[1].instance == 0);
    });
    test("remove stronger retains weaker and unrelated ward and completed damage", [] {
        Fixture f;
        f.cast(plan(contact("fire damage 4 dp instant")), {f.a, f.target, {}});
        f.add();
        f.cast(plan(slow(500, 3000)), {f.b, f.target, {}});
        f.cast(plan(contact("ward absorb 8 dp for 4000 ms")), {f.a, f.target, {}});
        auto wards = take(f.host->inspect_mechanics());
        f.cast(plan(dispel()), f.removal(f.ref(2)));
        check(
            f.host->snapshot().actors[2].slow == R::Slow{750, 200} && f.host->snapshot().actors[2].data.health == 46 &&
            take(f.host->inspect_mechanics()) == wards
        );
        check(take(f.host->slows()).influences[0].instance == 1 && take(f.host->slows()).influences[1].instance == 0);
    });
    test("remove weaker retains stronger with exact independent expiry", [] {
        Fixture f;
        f.add();
        f.add(500, 3000);
        f.cast(plan(dispel()), f.removal(f.ref()));
        check(f.host->snapshot().actors[2].slow == R::Slow{500, 300});
        take(f.host->advance_to(299));
        check(f.host->snapshot().actors[2].speed_milli_mm_per_s() == 500000);
        take(f.host->advance_to(300));
        check(f.host->snapshot().actors[2].slow == R::Slow{});
    });
    test("stronger expires first and weaker remains until its own deadline", [] {
        Fixture f;
        f.add(500, 1000);
        f.add(750, 3000);
        take(f.host->advance_to(99));
        check(f.host->snapshot().actors[2].slow == R::Slow{500, 100});
        take(f.host->advance_to(100));
        check(f.host->snapshot().actors[2].slow == R::Slow{750, 300});
        take(f.host->advance_to(300));
        check(f.host->snapshot().actors[2].slow == R::Slow{});
    });
    test("weaker expires first without compounded or rounded speed", [] {
        Fixture f;
        f.add(750, 1000);
        f.add(333, 3000);
        take(f.host->advance_to(100));
        check(f.host->snapshot().actors[2].slow == R::Slow{333, 300} && f.host->snapshot().actors[2].speed_milli_mm_per_s() == 333000);
    });
    test("equal strength exact removal and simultaneous expiry", [] {
        Fixture f;
        f.add();
        f.add();
        auto r = f.ref();
        f.cast(plan(dispel()), f.removal(r));
        check(f.host->snapshot().actors[2].slow == R::Slow{750, 200});
        auto r2 = f.ref(2);
        take(f.host->advance_to(200));
        f.unchanged([&] { refuses(f.host->prepare(plan(dispel()), f.removal(r2)), R::Error::MissingInfluence); });
    });
    test("default foreign and wrong kind inputs refuse atomically", [] {
        Fixture f, other;
        f.add();
        other.add();
        auto foreign = other.ref();
        f.unchanged([&] { refuses(f.host->prepare(plan(dispel()), f.removal({})), R::Error::InvalidCapability); });
        f.unchanged([&] { refuses(f.host->prepare(plan(dispel()), f.removal(foreign)), R::Error::ForeignCapability); });
        auto r = f.ref();
        f.unchanged([&] { refuses(f.host->prepare(plan(dispel("attack-restriction")), f.removal(r)), R::Error::UnsupportedBinding); });
        refuses(f.host->bind_slow(1, f.a), R::Error::MissingInfluence);
        refuses(f.host->bind_slow(0, f.target), R::Error::MissingInfluence);
    });
    test("repeated expired and reused row never retarget", [] {
        Fixture f;
        f.add();
        auto r = f.ref();
        f.cast(plan(dispel()), f.removal(r));
        f.add(500);
        check(take(f.host->slows()).influences[0].instance == 2);
        f.unchanged([&] { refuses(f.host->prepare(plan(dispel()), f.removal(r)), R::Error::MissingInfluence); });
        auto r2 = f.ref(2);
        take(f.host->advance_to(200));
        f.add();
        f.unchanged([&] { refuses(f.host->prepare(plan(dispel()), f.removal(r2)), R::Error::MissingInfluence); });
    });
    test("actor removal and generation reuse retire exact contributions", [] {
        Fixture f;
        f.add();
        auto r = f.ref();
        ok(f.host->remove_actor(f.target));
        auto replacement = take(f.host->add_actor({{20, 0}, 60}));
        check(replacement.slot == f.target.slot && replacement.generation == 2);
        f.cast(plan(slow(500)), {f.a, replacement, {}});
        f.unchanged([&] { refuses(f.host->prepare(plan(dispel()), f.removal(r)), R::Error::MissingInfluence); });
        check(f.host->snapshot().actors[2].slow == R::Slow{500, 200});
    });
    test("bound identity confers no permission resources or schema authority", [] {
        Fixture f;
        f.add();
        auto r = f.ref();
        auto p = plan(dispel());
        auto ticket = take(f.host->prepare(p, f.removal(r)));
        auto caster = f.host->snapshot().actors[0].data;
        caster.can_cast = false;
        ok(f.host->update_actor(f.a, caster));
        f.unchanged([&] { refuses(f.host->release(ticket), R::Error::Permission); });
        caster.can_cast = true;
        caster.mana = 0;
        ok(f.host->update_actor(f.a, caster));
        f.unchanged([&] { refuses(f.host->release(ticket), R::Error::Resources); });
        caster.mana = 100;
        ok(f.host->update_actor(f.a, caster));
        auto c = game();
        c.operations[5].key = "changed.dispel";
        ok(f.host->replace_catalog(c));
        f.unchanged([&] { refuses(f.host->release(ticket), R::Error::SchemaMismatch); });
        ok(f.host->replace_catalog(game()));
        refuses(f.host->release(ticket), R::Error::StaleTicket);
        f.cast(p, f.removal(r));
        check(f.host->snapshot().actors[0].data.mana == 90);
    });
    test("prepared reference expiring rechecks live identity before staleness", [] {
        Fixture f;
        f.add(500, 1000);
        auto ticket = take(f.host->prepare(plan(dispel()), f.removal(f.ref())));
        take(f.host->advance_to(100));
        f.unchanged([&] { refuses(f.host->release(ticket), R::Error::MissingInfluence); });
        ok(f.host->discard(ticket));
    });
    test("contribution capacity zero exact and one beyond preserve full composed payload", [] {
        R::HostLimits limits;
        limits.statuses = 0;
        Fixture zero(limits);
        zero.unchanged([&] { refuses(zero.host->prepare(plan(slow()), {zero.a, zero.target, {}}), R::Error::StatusLimit); });
        limits.statuses = 2;
        Fixture f(limits);
        f.add();
        f.add(500);
        f.unchanged([&] { refuses(f.host->prepare(plan(slow()), {f.a, f.target, {}}), R::Error::StatusLimit); });
        f.cast(plan(dispel()), f.removal(f.ref()));
        f.add(600);
        check(take(f.host->slows()).instances_issued == 3);
    });
    test("instance horizon refuses without debit and never wraps after expiry", [] {
        R::HostLimits limits;
        limits.max_slow_instance = 1;
        Fixture f(limits);
        f.add();
        take(f.host->advance_to(200));
        f.unchanged([&] { refuses(f.host->prepare(plan(slow()), {f.a, f.target, {}}), R::Error::InstanceLimit); });
        limits.max_slow_instance = 1000001;
        refuses(R::ReferenceHost::create(game(), limits), R::Error::InvalidLimits);
    });
    test("paid delayed action retains origin and expires before same tick replacement", [] {
        Fixture f;
        f.add(750, 1000);
        auto old = f.ref();
        auto queued = f.cast(plan(burst()), {f.b, {}, R::Point{20, 0}});
        check(queued->events[0].action == 2 && f.host->snapshot().actors[1].data.mana == 980);
        take(f.host->advance_to(100));
        auto row = take(f.host->slows()).influences[0];
        check(
            row.instance == 2 && row.source == R::SlowSource{f.b, 2} && row.effect_index == 1 && row.onset_tick == 100 &&
            row.until_tick == 400
        );
        check(f.host->snapshot().actors[2].data.health == 46 && f.host->snapshot().actors[2].slow == R::Slow{500, 400});
        f.unchanged([&] { refuses(f.host->prepare(plan(dispel()), f.removal(old)), R::Error::MissingInfluence); });
    });
    test("late delayed capacity refusal rolls back damage queue and identities", [] {
        R::HostLimits limits;
        limits.statuses = 1;
        Fixture f(limits);
        f.add(750, 3000);
        f.cast(plan(burst()), {f.b, {}, R::Point{20, 0}});
        f.unchanged([&] { refuses(f.host->advance_to(100), R::Error::StatusLimit); });
        f.cast(plan(dispel()), f.removal(f.ref()));
        take(f.host->advance_to(100));
        check(take(f.host->slows()).instances_issued == 2 && f.host->snapshot().actors[2].data.health == 46);
    });
    test("item touch owns exact item fact and selective removal preserves heal charge and receipt", [] {
        Fixture f;
        f.add();
        auto id = take(f.host->install(plan(item()), f.a));
        auto event = take(f.host->admit_touch(17, id, f.target));
        take(f.host->touch(event));
        auto rows = take(f.host->slows());
        check(
            rows.influences[1].source == R::SlowSource{{}, 0, id, 17} && rows.influences[1].effect_index == 1 &&
            f.host->snapshot().actors[2].data.health == 53
        );
        auto items = f.host->items();
        f.cast(plan(dispel()), f.removal(f.ref(2)));
        check(
            f.host->items() == items && f.host->snapshot().actors[2].data.health == 53 &&
            f.host->snapshot().actors[2].slow == R::Slow{750, 200}
        );
        refuses(f.host->touch(event), R::Error::RepeatedEvent);
    });
    test("checkpoint owns active and pending plans item facts and renews all capabilities", [] {
        R::Checkpoint cp;
        R::InfluenceRef old;
        R::Ticket ticket;
        R::ActorId a, target;
        {
            Fixture f;
            a = f.a;
            target = f.target;
            f.add();
            old = f.ref();
            auto id = take(f.host->install(plan(item()), f.a));
            take(f.host->touch(take(f.host->admit_touch(17, id, f.target))));
            f.cast(plan(burst()), {f.b, {}, R::Point{20, 0}});
            ticket = take(f.host->prepare(plan(dispel()), f.removal(old)));
            cp = take(f.host->checkpoint());
        }
        auto host = take(R::ReferenceHost::create(game()));
        ok(host->restore(cp));
        refuses(host->release(ticket), R::Error::ForeignTicket);
        refuses(host->prepare(plan(dispel()), {a, {}, {}, {}, {}, old}), R::Error::ForeignCapability);
        auto current = take(host->bind_slow(1, target));
        take(host->release(take(host->prepare(plan(dispel()), {a, {}, {}, {}, {}, current}))));
        take(host->advance_to(100));
        check(host->snapshot().actors[target.slot].data.health == 49 && host->snapshot().actors[target.slot].slow == R::Slow{500, 400});
        ok(host->restore(cp));
        refuses(host->prepare(plan(dispel()), {a, {}, {}, {}, {}, current}), R::Error::ForeignCapability);
        check(take(host->slows()).instances_issued == 2 && host->snapshot().actors[target.slot].data.health == 53);
    });
    test("departed caster or removed item does not erase active provenance", [] {
        Fixture f;
        f.add();
        auto id = take(f.host->install(plan(item()), f.a));
        take(f.host->touch(take(f.host->admit_touch(17, id, f.target))));
        ok(f.host->remove_item(id));
        ok(f.host->remove_actor(f.a));
        auto cp = take(f.host->checkpoint());
        ok(f.host->restore(cp));
        check(take(f.host->slows()).influences[0].source.caster == f.a);
        f.cast(plan(dispel()), {f.b, {}, {}, {}, {}, f.ref()});
        check(f.host->snapshot().actors[2].slow == R::Slow{600, 400});
    });
    test("sixteen overlapping rows exact capacity reuse and checkpoint", [] {
        Fixture f;
        for (std::uint32_t i = 0; i < 16; ++i)
            f.add(900 - i * 10, 4000);
        check(take(f.host->slows()).instances_issued == 16 && f.host->snapshot().actors[2].slow == R::Slow{750, 400});
        f.unchanged([&] { refuses(f.host->prepare(plan(slow()), {f.a, f.target, {}}), R::Error::StatusLimit); });
        auto old = f.ref(16);
        f.cast(plan(dispel()), f.removal(old));
        check(f.host->snapshot().actors[2].slow == R::Slow{760, 400});
        f.add(500, 4000);
        check(take(f.host->slows()).influences[15].instance == 17 && f.host->snapshot().actors[2].slow == R::Slow{500, 400});
        f.unchanged([&] { refuses(f.host->prepare(plan(dispel()), f.removal(old)), R::Error::MissingInfluence); });
        auto cp = take(f.host->checkpoint());
        ok(f.host->restore(cp));
        check(take(f.host->slows()).instances_issued == 17);
    });
    test("one multi-target cast owns separate influences and selective removal", [] {
        Fixture f;
        auto other = take(f.host->add_actor({{30, 0}, 60}));
        f.cast(plan(burst()), {f.a, {}, R::Point{20, 0}});
        take(f.host->advance_to(100));
        auto rows = take(f.host->slows());
        check(
            rows.influences[0].source == R::SlowSource{f.a, 1} && rows.influences[1].source == R::SlowSource{f.a, 1} &&
            rows.influences[0].actor == f.target && rows.influences[1].actor == other
        );
        f.cast(plan(dispel()), f.removal(f.ref()));
        check(
            f.host->snapshot().actors[f.target.slot].slow == R::Slow{} && f.host->snapshot().actors[f.target.slot].data.health == 46 &&
            f.host->snapshot().actors[other.slot].slow == R::Slow{500, 400} && f.host->snapshot().actors[other.slot].data.health == 56
        );
    });
    test("late multi-target instance exhaustion rolls back earlier damage identity and expiry", [] {
        R::HostLimits limits;
        limits.max_slow_instance = 2;
        Fixture f(limits);
        auto other = take(f.host->add_actor({{30, 0}, 60}));
        f.add(750, 500);
        f.cast(plan(burst()), {f.a, {}, R::Point{20, 0}});
        f.unchanged([&] { refuses(f.host->advance_to(100), R::Error::InstanceLimit); });
        check(
            f.host->snapshot().tick == 0 && f.host->snapshot().actors[other.slot].data.health == 60 &&
            f.host->snapshot().actors[f.target.slot].data.health == 50 && take(f.host->slows()).instances_issued == 1
        );
        ok(f.host->remove_actor(other));
        take(f.host->advance_to(100));
        check(take(f.host->slows()).instances_issued == 2 && f.host->snapshot().actors[2].data.health == 46);
    });
    test("late action refusal preserves prepared ticket contribution and payment", [] {
        R::HostLimits limits;
        limits.max_action = 1;
        Fixture f(limits);
        f.add();
        auto ticket = take(f.host->prepare(plan(dispel()), f.removal(f.ref())));
        f.unchanged([&] { refuses(f.host->release(ticket), R::Error::ActionLimit); });
        ok(f.host->discard(ticket));
        check(f.host->snapshot().actors[0].data.mana == 990);
    });
    test("zero instance horizon and neutral factor retain explicit identity", [] {
        R::HostLimits limits;
        limits.max_slow_instance = 0;
        Fixture zero(limits);
        zero.unchanged([&] { refuses(zero.host->prepare(plan(slow()), {zero.a, zero.target, {}}), R::Error::InstanceLimit); });
        Fixture f;
        f.add(1000, 1000);
        check(take(f.host->slows()).influences[0].instance == 1 && f.host->snapshot().actors[2].slow == R::Slow{1000, 100});
        f.cast(plan(dispel()), f.removal(f.ref()));
        check(f.host->snapshot().actors[2].slow == R::Slow{});
    });
    test("checkpoint schema and limit refusal preserve active authority", [] {
        Fixture f;
        f.add();
        auto ref = f.ref();
        auto cp = take(f.host->checkpoint());
        auto catalog = game();
        catalog.operations[2].max_duration_ticks = 199;
        ok(f.host->replace_catalog(catalog));
        f.unchanged([&] { refuses(f.host->checkpoint(), R::Error::SchemaMismatch); });
        f.unchanged([&] { refuses(f.host->restore(cp), R::Error::IncompatibleCheckpoint); });
        ok(f.host->replace_catalog(game()));
        R::HostLimits limits;
        limits.statuses = 15;
        Fixture other(limits);
        other.unchanged([&] { refuses(other.host->restore(cp), R::Error::IncompatibleCheckpoint); });
        f.cast(plan(dispel()), f.removal(ref));
    });
    test("exact work refusal preserves exact instance and payment", [] {
        Fixture measure;
        measure.add();
        auto catalog = game();
        catalog.operations[5].key = std::string(64, 'd');
        auto p = take(compile(dispel(), catalog));
        ok(measure.host->replace_catalog(catalog));
        const auto work = take(measure.host->preview(p, measure.removal(measure.ref()))).usage.work;
        // A valid long dispel key isolates this boundary above the setup's Slow application cost.
        R::HostLimits limits;
        limits.work = work - 1;
        Fixture f(limits);
        f.add();
        ok(f.host->replace_catalog(catalog));
        const auto before = f.host->snapshot();
        const auto rows = take(f.host->slows());
        refuses(f.host->prepare(p, f.removal(f.ref())), R::Error::WorkLimit);
        check(before == f.host->snapshot() && rows == take(f.host->slows()));
        limits.work = work;
        Fixture exact(limits);
        exact.add();
        ok(exact.host->replace_catalog(catalog));
        auto ticket = take(exact.host->prepare(p, exact.removal(exact.ref())));
        check(take(exact.host->release(ticket))->usage.work == work);
        std::cout << "MEASURE Slow dispel work " << work << " one-short " << work - 1 << '\n';
    });
    std::cout << "MEASURE inline contribution bytes " << sizeof(R::SlowInfluence) << " snapshot bytes " << sizeof(R::SlowSnapshot)
              << " host bytes " << sizeof(R::ReferenceHost) << '\n';
    std::cout << passed << " M9 Slow cases passed; " << failed << " failed\n";
    return failed ? 1 : 0;
}
