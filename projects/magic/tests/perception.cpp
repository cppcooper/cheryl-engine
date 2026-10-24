#include <magic/learning.h>
#include <magic/reference-host.h>

#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
    using namespace Magic;
    namespace R = Magic::Reference;
    namespace L = Magic::Learning;
    int passed = 0, failed = 0;
    void check(bool value, std::string_view why = "literal expectation") {
        if (!value)
            throw std::runtime_error(std::string(why));
    }
    template <class T, class E> T take(std::expected<T, E> v) {
        if (!v)
            throw std::runtime_error("unexpected failure");
        return std::move(*v);
    }
    template <class E> void ok(std::expected<void, E> v) {
        check(bool(v), "expected success");
    }
    template <class T, class E, class C> void rejects(const std::expected<T, E>& v, C code) {
        check(!v, "expected refusal");
        if (v.error().code != code)
            throw std::runtime_error("wrong refusal code");
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
        c.operations = {
            {Primitive::Heal, "game.heal"},
            {Primitive::Fire, "game.fire"},
            {Primitive::Slow, "game.slow"},
            {Primitive::Disguise, "game.disguise"},
            {Primitive::Discern, "game.discern"}
        };
        return c;
    }
    std::string read(std::string_view name) {
        std::ifstream f(std::string(MAGIC_FIXTURE_DIR) + "/" + std::string(name));
        check(f.good(), "fixture open");
        std::string s(std::istreambuf_iterator<char>(f), {});
        if (s.ends_with('\n'))
            s.pop_back();
        return s;
    }
    std::string replace(std::string s, std::string_view from, std::string_view to) {
        const auto pos = s.find(from);
        check(pos != s.npos, "editable phrase present");
        s.replace(pos, from.size(), to);
        return s;
    }
    std::string show(bool immediate = false) {
        auto s = read("m6/disguise.common");
        return immediate ? replace(s, "after 1000 ms", "after 0 ms") : s;
    }
    std::string sense() {
        return read("m6/discern.common");
    }
    PlanHandle plan(std::string_view s) {
        auto p = compile(s, game());
        if (!p)
            throw std::runtime_error(p.error().message);
        return *p;
    }
    struct Scene {
        std::unique_ptr<R::ReferenceHost> h;
        R::ActorId caster, a, b;
        R::ObjectId arch;
        explicit Scene(R::HostLimits limits = {}, bool reverse = false) {
            h = take(R::ReferenceHost::create(game(), limits));
            caster = take(h->add_actor({{1000, 0}, 100, 100, 1000, 1000, false, true, true}));
            auto add = [&](R::Point p) { return take(h->add_actor({p, 50, 100, 0, 1000, true, true, false})); };
            if (reverse) {
                b = add({100, 0});
                a = add({251, 0});
            } else {
                a = add({251, 0});
                b = add({100, 0});
            }
            arch = take(h->add_object({Appearance::OpenArch, true}));
        }
        R::Observed cast(const PlanHandle& p, R::Point aim = {}, std::optional<R::ObjectId> subject = {}) {
            return take(h->release(take(h->prepare(p, {caster, {}, aim, subject ? subject : arch}))));
        }
        void move(R::ActorId id, R::Point p) {
            auto s = h->snapshot().actors[id.slot].data;
            s.position = p;
            ok(h->update_actor(id, s));
        }
        void enter() {
            move(a, {100, 0});
            move(b, {251, 0});
        }
        template <class F> void unchanged(F body) {
            auto w = h->snapshot();
            auto i = h->items();
            auto p = take(h->inspect_perception());
            body();
            check(
                w == h->snapshot() && i == h->items() && p == take(h->inspect_perception()),
                "complete state/clock/funds/queue/knowledge preserved"
            );
        }
        R::Perceived view(R::ActorId observer) { return take(h->perceive(observer, arch)); }
    };
    std::string annular(std::string s) {
        s = replace(s, "disk 250 mm", "annulus excluding inner disk 100 mm inclusive within outer disk 250 mm inclusive");
        return replace(s, "queried at activation inclusive ;", "queried at activation ;");
    }
    std::string single(std::string s) {
        return replace(s, "disk 250 mm", "disk 1 mm");
    }
    void literal_health(const Scene& f, std::uint32_t mana) {
        const auto w = f.h->snapshot();
        check(w.actors[f.caster.slot].data.mana == mana && w.actors[f.a.slot].data.health == 50 && w.actors[f.b.slot].data.health == 50);
        check(w.actors[f.a.slot].slow == R::Slow{} && w.actors[f.b.slot].slow == R::Slow{});
    }
}
int main() {
    test("independent complete typed Common meaning roundtrip and requirements", [] {
        auto p = plan(show());
        Meaning expected;
        expected.delivery = Delivery::Burst;
        expected.onset_ticks = 100;
        expected.bindings = {{"audience", Query::EligibleDisk, "G0", 250, BindingTime::Release, BindingTime::Activation, true}};
        expected.effects = {
            {Primitive::Disguise, "game.disguise", 0, 0, 0, VisualDisguise{SubjectRole::CapturedObject, Appearance::StoneWall, 200}}
        };
        check(p->meaning() == expected && p->common() == show());
        check(plan(p->common())->meaning() == expected);
        check(p->requirements() == (rule_bit(Rule::TargetBridge) | rule_bit(Rule::CommonDisguise)));
        auto d = plan(sense());
        expected.onset_ticks = 0;
        expected.bindings[0].radius_mm = 1;
        expected.effects = {{Primitive::Discern, "game.discern", 0, 0, 0, Discernment{SubjectRole::CapturedObject}}};
        check(d->meaning() == expected && d->common() == sense() && plan(d->common())->meaning() == expected);
        check(d->requirements() == (rule_bit(Rule::TargetBridge) | rule_bit(Rule::CommonDiscern)));
    });
    test("appearance duration onset and annular audience edit independent roles", [] {
        const auto p = plan(show());
        auto m = p->meaning();
        auto appearance = plan(replace(show(), "stone-wall", "open-arch"));
        auto expected = m;
        std::get<VisualDisguise>(expected.effects[0].payload).form = Appearance::OpenArch;
        check(appearance->meaning() == expected);
        auto duration = plan(replace(show(), "2000 ms", "1500 ms"));
        expected = m;
        std::get<VisualDisguise>(expected.effects[0].payload).duration_ticks = 150;
        check(duration->meaning() == expected);
        auto onset = plan(replace(show(), "1000 ms", "500 ms"));
        expected = m;
        expected.onset_ticks = 50;
        check(onset->meaning() == expected);
        auto ring = plan(annular(show()));
        expected = m;
        expected.bindings[0].query = Query::EligibleAnnulus;
        expected.bindings[0].inner_radius_mm = 100;
        check(ring->meaning() == expected && plan(ring->common())->meaning() == expected);
        check(ring->requirements() == (p->requirements() | rule_bit(Rule::AnnularTarget)));
    });
    test("owned role appearance unit range and scope diagnostics", [] {
        const std::pair<std::string, Code> cases[] = {
            {replace(show(), "show subject", "show actor"), Code::RoleMismatch},
            {replace(show(), "stone-wall", "dragon"), Code::UnknownPhrase},
            {replace(show(), "2000 ms", "2 hp"), Code::WrongDimension},
            {replace(show(), "2000 ms", "1 ms"), Code::Inexact},
            {replace(show(), "2000 ms", "2 minutes"), Code::UnsupportedUnit},
            {replace(show(), "2000 ms", "0 ms"), Code::Range},
            {replace(show(), "2000 ms", "-1 ms"), Code::Range},
            {replace(show(), "2000 ms", "999999999999999999999999999999 s"), Code::Range},
            {replace(show(), "frozen at activation", "frozen at release"), Code::Malformed}
        };
        for (auto [source, code] : cases) {
            auto r = compile(source, game());
            check(!r);
            check(r.error().code == code);
            auto diag = r.error();
            source.clear();
            check(!diag.message.empty() && diag.span.length > 0 && !diag.excerpt.empty());
        }
    });
    test("perception bundles and engraving refuse before any payment", [] {
        auto s = show(true);
        const auto end = s.rfind("  }");
        s.insert(end, "    on audience bridge Common@1 { heal 12 hp instant } ;\n");
        auto r = compile(s, game());
        check(!r && r.error().code == Code::UnsupportedComposition);
        auto two = show(true);
        two.insert(
            two.rfind("  }"),
            "    on audience bridge Common@1 { discern subject captured at release rechecked at activation for this audience instant } ;\n"
        );
        r = compile(two, game());
        check(!r && r.error().code == Code::UnsupportedComposition);
        r = compile(replace(show(true), "spoken release via burst", "engraved item-touch charges 1 release via item-event"), game());
        check(!r);
        Scene f;
        f.unchanged([&] { rejects(f.h->install(plan(show()), f.caster), R::Error::UnsupportedEntry); });
    });
    test("literal before paid release onset expiry in both actor storage orders", [] {
        for (bool reverse : {false, true}) {
            Scene f({}, reverse);
            check(f.view(f.a) == R::Perceived{f.a, f.arch, Appearance::OpenArch, 0});
            auto before = f.h->snapshot();
            auto preview = take(f.h->preview(plan(show()), {f.caster, {}, R::Point{}, f.arch}));
            check(preview.mana_cost == 10 && preview.due_tick == 100 && before == f.h->snapshot());
            f.cast(plan(show()));
            literal_health(f, 990);
            check(take(f.h->inspect_perception()).instances_issued == 0);
            const auto pending = f.h->snapshot().pending[0];
            check(pending.present && pending.due_tick == 100 && pending.aim == R::Point{} && pending.subject == f.arch);
            f.enter();
            take(f.h->advance_to(99));
            check(f.view(f.a).form == Appearance::OpenArch);
            take(f.h->advance_to(100));
            check(
                f.view(f.a) == R::Perceived{f.a, f.arch, Appearance::StoneWall, 100} &&
                f.view(f.b) == R::Perceived{f.b, f.arch, Appearance::OpenArch, 100}
            );
            auto p = take(f.h->inspect_perception());
            check(p.objects[0].data == R::ObjectSpec{Appearance::OpenArch, true});
            check(
                p.disguises[0].instance == 1 && p.disguises[0].subject == f.arch && p.disguises[0].onset_tick == 100 &&
                p.disguises[0].until_tick == 300
            );
            check(p.disguises[0].audience[f.a.slot] == f.a && p.disguises[0].audience[f.b.slot] == R::ActorId{});
            f.move(f.a, {900, 0});
            f.move(f.b, {0, 0}); // Frozen audience, not a continuously queried field.
            check(f.view(f.a).form == Appearance::StoneWall && f.view(f.b).form == Appearance::OpenArch);
            take(f.h->advance_to(299));
            check(f.view(f.a).form == Appearance::StoneWall);
            auto old = f.view(f.a);
            take(f.h->advance_to(300));
            check(f.view(f.a).form == Appearance::OpenArch);
            check(old == R::Perceived{f.a, f.arch, Appearance::StoneWall, 299});
            literal_health(f, 990);
            check(take(f.h->inspect_perception()).disguises[0].instance == 0);
        }
    });
    test("discernment records exact A-only fact and no ordinary query grants knowledge", [] {
        Scene f;
        f.enter();
        f.cast(plan(show(true)));
        auto old = f.view(f.a);
        check(take(f.h->knowledge(f.a)).count == 0 && take(f.h->knowledge(f.b)).count == 0);
        f.cast(plan(sense()), {100, 0});
        literal_health(f, 980);
        auto a = take(f.h->knowledge(f.a));
        check(a.count == 1);
        check(a.witnesses[0] == R::Witness{f.a, f.arch, Appearance::OpenArch, true, 0, 4, 1});
        check(take(f.h->knowledge(f.b)).count == 0 && f.view(f.a).form == Appearance::OpenArch);
        check(old == R::Perceived{f.a, f.arch, Appearance::StoneWall, 0});
        f.unchanged([&] { rejects(f.h->prepare(plan(sense()), {f.caster, {}, R::Point{100, 0}, f.arch}), R::Error::NoDisguise); });
    });
    test("no applicable disguise or partial audience discernment refuses all", [] {
        Scene f;
        f.unchanged([&] { rejects(f.h->prepare(plan(sense()), {f.caster, {}, R::Point{100, 0}, f.arch}), R::Error::NoDisguise); });
        f.enter();
        f.cast(plan(single(show(true))), {100, 0});
        f.move(f.b, {101, 0});
        auto broad = plan(replace(sense(), "disk 1 mm", "disk 250 mm"));
        f.unchanged([&] { rejects(f.h->prepare(broad, {f.caster, {}, R::Point{100, 0}, f.arch}), R::Error::NoDisguise); });
        check(take(f.h->knowledge(f.a)).count == 0);
    });
    test("dated witnesses survive changed physical facts and do not reveal future instances", [] {
        Scene f;
        f.enter();
        f.cast(plan(show(true)));
        f.cast(plan(sense()), {100, 0});
        ok(f.h->update_object(f.arch, {Appearance::StoneWall, false}));
        check(f.view(f.a).form == Appearance::StoneWall);
        auto known = take(f.h->knowledge(f.a));
        check(
            known.witnesses[0].observed_form == Appearance::OpenArch && known.witnesses[0].observed_passable &&
            known.witnesses[0].fact_revision == 4
        );
        take(f.h->advance_to(200));
        ok(f.h->update_object(f.arch, {Appearance::OpenArch, true}));
        f.cast(plan(show(true)));
        auto p = take(f.h->inspect_perception());
        check(p.disguises[0].instance == 2 && p.instances_issued == 2);
        check(f.view(f.a).form == Appearance::StoneWall && take(f.h->knowledge(f.a)).count == 1);
        f.cast(plan(sense()), {100, 0});
        known = take(f.h->knowledge(f.a));
        check(known.count == 2 && known.witnesses[1].instance == 2 && known.witnesses[1].observation_tick == 200);
    });
    test("live symbolic key primitive appearance duration and permissions rechecked", [] {
        for (unsigned variation = 0; variation < 6; ++variation) {
            Scene f;
            auto p = plan(show());
            auto ticket = take(f.h->prepare(p, {f.caster, {}, R::Point{}, f.arch}));
            auto c = game();
            if (variation == 0)
                c.operations[3].key = "game.changed";
            if (variation == 1)
                std::swap(c.operations[3].primitive, c.operations[4].primitive);
            if (variation == 2)
                c.operations[3].appearances = 1;
            if (variation == 3)
                c.operations[3].max_duration_ticks = 199;
            if (variation == 4)
                c.burst = false;
            if (variation == 5) {
                auto a = f.h->snapshot().actors[f.caster.slot].data;
                a.can_cast = false;
                ok(f.h->update_actor(f.caster, a));
            } else
                ok(f.h->replace_catalog(c));
            f.unchanged([&] { rejects(f.h->release(ticket), variation == 5 ? R::Error::Permission : R::Error::SchemaMismatch); });
        }
        auto c = game();
        c.operations[3].appearances = 1;
        auto p = compile(show(), c);
        check(!p && p.error().code == Code::UnsupportedOperation);
    });
    test("discern current unknown key and disabled capability refuse without information", [] {
        Scene f;
        f.enter();
        f.cast(plan(show(true)));
        auto p = plan(sense());
        auto ticket = take(f.h->prepare(p, {f.caster, {}, R::Point{100, 0}, f.arch}));
        auto c = game();
        c.operations[4].key = "unknown.discern";
        ok(f.h->replace_catalog(c));
        f.unchanged([&] { rejects(f.h->release(ticket), R::Error::SchemaMismatch); });
        check(take(f.h->knowledge(f.a)).count == 0 && f.view(f.a).form == Appearance::StoneWall);
    });
    test("default foreign stale copied and repeated authority cannot act", [] {
        Scene f, other;
        auto p = plan(show());
        f.unchanged([&] { rejects(f.h->release({}), R::Error::InvalidTicket); });
        auto foreign = take(other.h->prepare(p, {other.caster, {}, R::Point{}, other.arch}));
        f.unchanged([&] { rejects(f.h->release(foreign), R::Error::ForeignTicket); });
        auto t = take(f.h->prepare(p, {f.caster, {}, R::Point{}, f.arch}));
        f.move(f.a, {200, 0});
        f.unchanged([&] { rejects(f.h->release(t), R::Error::StaleTicket); });
        t = take(f.h->prepare(p, {f.caster, {}, R::Point{}, f.arch}));
        auto copy = t;
        take(f.h->release(t));
        f.unchanged([&] { rejects(f.h->release(copy), R::Error::ConsumedTicket); });
    });
    test("subject capture refuses missing generation at release and delayed activation", [] {
        Scene f;
        auto p = plan(show());
        auto request = R::Request{f.caster, {}, R::Point{}, f.arch};
        auto ticket = take(f.h->prepare(p, request));
        ok(f.h->remove_object(f.arch));
        auto replacement = take(f.h->add_object({Appearance::StoneWall, false}));
        check(replacement == R::ObjectId{0, 2});
        f.unchanged([&] { rejects(f.h->release(ticket), R::Error::MissingObject); });
        Scene delayed;
        delayed.cast(p);
        ok(delayed.h->remove_object(delayed.arch));
        take(delayed.h->add_object({}));
        delayed.unchanged([&] { rejects(delayed.h->advance_to(100), R::Error::MissingObject); });
        literal_health(delayed, 990);
        check(delayed.h->snapshot().pending[0].present);
        take(delayed.h->cancel(1));
        literal_health(delayed, 990);
    });
    test("observer removal reuse and subject removal retire only matching contributions", [] {
        Scene f;
        f.enter();
        f.cast(plan(show(true)));
        f.cast(plan(sense()), {100, 0});
        auto historical = take(f.h->knowledge(f.a));
        ok(f.h->remove_actor(f.a));
        auto replacement = take(f.h->add_actor({{100, 0}, 50, 100, 0, 1000, true, true, false}));
        check(replacement.slot == f.a.slot && replacement.generation == 2);
        check(take(f.h->perceive(replacement, f.arch)).form == Appearance::OpenArch && take(f.h->knowledge(replacement)).count == 0);
        check(historical.witnesses[0].observer == f.a && take(f.h->inspect_perception()).witness_count == 1);
        rejects(f.h->perceive(f.a, f.arch), R::Error::MissingActor);
        auto second = take(f.h->add_object({}));
        f.cast(plan(show(true)), {}, second);
        ok(f.h->remove_object(f.arch));
        check(take(f.h->inspect_perception()).disguises[0].subject == second);
        auto reused = take(f.h->add_object({Appearance::StoneWall, false}));
        check(reused.generation == 2);
        check(take(f.h->inspect_perception()).witnesses[0].subject == f.arch);
    });
    test("eligibility at activation and frozen audience after movement", [] {
        Scene f;
        f.cast(plan(show()));
        f.enter();
        auto a = f.h->snapshot().actors[f.a.slot].data;
        a.eligible = false;
        ok(f.h->update_actor(f.a, a));
        f.unchanged([&] { rejects(f.h->advance_to(100), R::Error::EmptySelection); });
        a.eligible = true;
        ok(f.h->update_actor(f.a, a));
        take(f.h->advance_to(100));
        f.move(f.a, {999, 0});
        f.move(f.b, {0, 0});
        check(f.view(f.a).form == Appearance::StoneWall && f.view(f.b).form == Appearance::OpenArch);
    });
    test("same subject audience overlap refuses while disjoint audiences and subjects coexist", [] {
        for (bool reverse : {false, true}) {
            Scene f({}, reverse);
            f.move(f.a, {0, 0});
            f.move(f.b, {100, 0});
            auto narrow = plan(single(show(true)));
            f.cast(narrow, {0, 0});
            f.unchanged([&] { rejects(f.h->prepare(narrow, {f.caster, {}, R::Point{0, 0}, f.arch}), R::Error::Overlap); });
            f.cast(narrow, {100, 0});
            auto object = take(f.h->add_object({}));
            f.cast(narrow, {100, 0}, object);
            auto p = take(f.h->inspect_perception());
            check(p.instances_issued == 3 && p.disguises[0].audience[f.a.slot] == f.a && p.disguises[1].audience[f.b.slot] == f.b);
        }
    });
    test("delayed future overlap capacity intentionally deferred and failed advance preserves paid queue", [] {
        Scene f;
        f.enter();
        f.cast(plan(show()));
        f.cast(plan(show(true)));
        f.unchanged([&] { rejects(f.h->advance_to(100), R::Error::Overlap); });
        literal_health(f, 980);
        check(f.h->snapshot().pending[0].present);
        take(f.h->cancel(1));
        literal_health(f, 980);
        take(f.h->advance_to(200));
        R::HostLimits l;
        l.disguises = 0;
        Scene cap(l);
        cap.cast(plan(show()));
        cap.enter();
        cap.unchanged([&] { rejects(cap.h->advance_to(100), R::Error::InstanceLimit); });
        literal_health(cap, 990);
    });
    test("expiry precedes same-tick activation without stale expiry removing replacement", [] {
        Scene f;
        f.enter();
        f.cast(plan(show(true)));
        auto later = plan(replace(show(), "1000 ms", "2000 ms"));
        f.cast(later);
        take(f.h->advance_to(200));
        check(f.view(f.a).form == Appearance::StoneWall);
        auto p = take(f.h->inspect_perception());
        check(p.disguises[0].instance == 2 && p.disguises[0].until_tick == 400);
        take(f.h->advance_to(300));
        check(f.view(f.a).form == Appearance::StoneWall);
        take(f.h->advance_to(400));
        check(f.view(f.a).form == Appearance::OpenArch);
    });
    test("object count generation revision and fixed ceiling refusal", [] {
        for (std::size_t cap : {0u, 1u, 4u}) {
            R::HostLimits l;
            l.objects = cap;
            auto h = take(R::ReferenceHost::create(game(), l));
            for (std::size_t i = 0; i < cap; ++i)
                check(take(h->add_object({})) == R::ObjectId{static_cast<std::uint32_t>(i), 1});
            auto before = h->snapshot();
            rejects(h->add_object({}), R::Error::ObjectLimit);
            check(before == h->snapshot());
        }
        R::HostLimits l;
        l.max_generation = 1;
        Scene f(l);
        ok(f.h->remove_object(f.arch));
        f.unchanged([&] { rejects(f.h->add_object({}), R::Error::GenerationLimit); });
        l = {};
        l.max_revision = 4;
        Scene revision(l);
        revision.unchanged([&] {
            rejects(revision.h->update_object(revision.arch, {Appearance::StoneWall, false}), R::Error::RevisionLimit);
        });
        for (unsigned which = 0; which < 4; ++which) {
            l = {};
            if (which == 0)
                l.objects = 5;
            if (which == 1)
                l.disguises = 9;
            if (which == 2)
                l.witnesses = 17;
            if (which == 3)
                l.max_instance = 1'000'001;
            rejects(R::ReferenceHost::create(game(), l), R::Error::InvalidLimits);
        }
    });
    test("eight live instance slots exact and one-short; disjoint audiences bound storage", [] {
        for (std::size_t cap : {7u, 8u}) {
            R::HostLimits l;
            l.disguises = cap;
            Scene f(l);
            f.move(f.a, {0, 0});
            f.move(f.b, {100, 0});
            std::array<R::ActorId, 9> actors{};
            actors[0] = f.a;
            actors[1] = f.b;
            for (std::size_t i = 2; i < 9; ++i)
                actors[i] = take(f.h->add_actor({{static_cast<std::int32_t>(i * 100), 0}, 50, 100, 0, 1000, true, true, false}));
            auto p = plan(single(show(true)));
            for (std::size_t i = 0; i < cap; ++i)
                f.cast(p, {static_cast<std::int32_t>(i * 100), 0});
            check(take(f.h->inspect_perception()).instances_issued == cap);
            f.unchanged([&] {
                rejects(
                    f.h->prepare(p, {f.caster, {}, R::Point{static_cast<std::int32_t>(cap * 100), 0}, f.arch}), R::Error::InstanceLimit
                );
            });
        }
    });
    test("instance identity exhaustion never wraps even after expiry", [] {
        R::HostLimits l;
        l.max_instance = 1;
        Scene f(l);
        f.enter();
        f.cast(plan(show(true)));
        take(f.h->advance_to(200));
        f.unchanged([&] { rejects(f.h->prepare(plan(show(true)), {f.caster, {}, R::Point{}, f.arch}), R::Error::InstanceLimit); });
        check(take(f.h->inspect_perception()).instances_issued == 1);
    });
    test("sixteen retained witnesses exact and one-short without silent eviction", [] {
        for (std::size_t cap : {0u, 1u, 15u, 16u}) {
            R::HostLimits l;
            l.witnesses = cap;
            Scene f(l);
            f.enter();
            auto p = plan(show(true));
            auto d = plan(sense());
            for (std::size_t i = 0; i < cap; ++i) {
                f.cast(p);
                f.cast(d, {100, 0});
                take(f.h->advance_to((i + 1) * 200));
            }
            f.cast(p);
            auto before = take(f.h->inspect_perception());
            f.unchanged([&] { rejects(f.h->prepare(d, {f.caster, {}, R::Point{100, 0}, f.arch}), R::Error::WitnessLimit); });
            check(before.witness_count == cap);
            if (cap)
                check(before.witnesses[0].instance == 1 && before.witnesses[cap - 1].instance == cap);
        }
    });
    test("paid delayed discernment witness exhaustion rolls back whole advance", [] {
        R::HostLimits l;
        l.witnesses = 0;
        Scene f(l);
        f.enter();
        f.cast(plan(show(true)));
        auto d = plan(replace(sense(), "after 0 ms", "after 1000 ms"));
        f.cast(d, {100, 0});
        f.unchanged([&] { rejects(f.h->advance_to(100), R::Error::WitnessLimit); });
        literal_health(f, 980);
        check(f.h->snapshot().tick == 0 && f.h->snapshot().pending[0].present && f.view(f.a).form == Appearance::StoneWall);
    });
    test("exact and insufficient funds with subject and pure preview", [] {
        for (std::uint32_t mana : {9u, 10u}) {
            Scene f;
            auto s = f.h->snapshot().actors[f.caster.slot].data;
            s.mana = mana;
            ok(f.h->update_actor(f.caster, s));
            if (mana == 9)
                f.unchanged([&] { rejects(f.h->prepare(plan(show()), {f.caster, {}, R::Point{}, f.arch}), R::Error::Resources); });
            else {
                auto before = f.h->snapshot();
                take(f.h->preview(plan(show()), {f.caster, {}, R::Point{}, f.arch}));
                check(before == f.h->snapshot());
                f.cast(plan(show()));
                literal_health(f, 0);
            }
        }
    });
    test("annular audience shares checked membership and old disk centre behavior", [] {
        Scene ring;
        ring.move(ring.a, {0, 0});
        ring.move(ring.b, {150, 200});
        ring.cast(plan(annular(show(true))));
        check(ring.view(ring.a).form == Appearance::OpenArch && ring.view(ring.b).form == Appearance::StoneWall);
        Scene disk;
        disk.move(disk.a, {0, 0});
        disk.move(disk.b, {150, 200});
        disk.cast(plan(show(true)));
        check(disk.view(disk.a).form == Appearance::StoneWall && disk.view(disk.b).form == Appearance::StoneWall);
    });
    test("selection selector and complete advance work exact and one-short", [] {
        Scene measured;
        measured.cast(plan(show()));
        measured.enter();
        const auto usage = take(measured.h->advance_to(100))->usage;
        std::cout << "perception activation work=" << usage.work << " selector=" << usage.selector_work
                  << " selected=" << usage.selected_targets << '\n';
        check(usage.selector_work == 16 && usage.selected_targets == 1);
        for (unsigned which = 0; which < 3; ++which)
            for (bool short_limit : {false, true}) {
                R::HostLimits l;
                if (which == 0)
                    l.work = usage.work - (short_limit ? 1 : 0);
                if (which == 1)
                    l.selector_work = short_limit ? 15 : 16;
                if (which == 2)
                    l.selected_targets = short_limit ? 0 : 1;
                Scene f(l);
                f.cast(plan(show()));
                f.enter();
                if (short_limit)
                    f.unchanged([&] {
                        rejects(
                            f.h->advance_to(100), which == 0   ? R::Error::WorkLimit
                                                  : which == 1 ? R::Error::SelectorLimit
                                                               : R::Error::TargetLimit
                        );
                    });
                else {
                    auto r = take(f.h->advance_to(100));
                    check(r->usage.work == usage.work);
                }
            }
    });
    test("time horizon exact versus one-short remains paid pending on failure", [] {
        for (std::uint64_t cap : {299u, 300u}) {
            R::HostLimits l;
            l.max_tick = cap;
            Scene f(l);
            f.cast(plan(show()));
            f.enter();
            if (cap == 299) {
                f.unchanged([&] { rejects(f.h->advance_to(100), R::Error::TimeLimit); });
                literal_health(f, 990);
            } else {
                take(f.h->advance_to(100));
                take(f.h->advance_to(300));
                check(f.view(f.a).form == Appearance::OpenArch);
            }
        }
    });
    test("later failed perception action preserves earlier scratch activation and clock", [] {
        Scene f;
        f.enter();
        auto one = plan(single(show()));
        f.cast(one, {100, 0});
        f.cast(one, {100, 0});
        f.unchanged([&] { rejects(f.h->advance_to(100), R::Error::Overlap); });
        literal_health(f, 980);
        check(
            take(f.h->inspect_perception()).instances_issued == 0 && f.h->snapshot().pending[0].present &&
            f.h->snapshot().pending[1].present
        );
        take(f.h->cancel(2));
        take(f.h->advance_to(100));
        check(f.view(f.a).form == Appearance::StoneWall);
    });
    test("compiler source token work retention exact while old output remains owned", [] {
        auto p = plan(show());
        const auto m = p->metrics();
        std::cout << "perception compiler tokens=" << m.tokens << " work=" << m.work << " retained=" << m.retained_bytes << '\n';
        for (unsigned which = 0; which < 4; ++which)
            for (bool short_limit : {false, true}) {
                Options o;
                if (which == 0)
                    o.limits.source_bytes = show().size() - (short_limit ? 1 : 0);
                if (which == 1)
                    o.limits.tokens = m.tokens - (short_limit ? 1 : 0);
                if (which == 2)
                    o.limits.work = m.work - (short_limit ? 1 : 0);
                if (which == 3)
                    o.limits.retained_bytes = m.retained_bytes - (short_limit ? 1 : 0);
                auto r = compile(show(), game(), o);
                if (short_limit)
                    check(!r);
                else
                    check(r && (*r)->meaning() == p->meaning());
            }
        check(p->common() == show());
    });
    test("M4 books distinguish recognized concepts knowledge and host permission", [] {
        auto learner = take(L::Learner::create(511));
        auto book = take(L::Spellbook::create());
        ok(book->add("scene", "Visual scene"));
        auto a = take(book->edit("scene", show(), "", game(), learner));
        check(a->problem() && a->problem()->code == L::Error::UnknownKnowledge && a->problem()->missing == rule_bit(Rule::CommonDisguise));
        auto known = take(L::Learner::create(all_rules));
        a = take(book->edit("scene", show(), "", game(), known));
        check(!a->problem() && a->accepted()->revision() == 1);
        auto old = a->accepted();
        auto d = take(book->edit("scene", sense(), "", game(), learner));
        check(d->problem()->missing == rule_bit(Rule::CommonDiscern) && d->accepted() == old);
        a = take(book->edit("scene", replace(show(), "stone-wall", "dragon"), "", game(), known));
        check(a->problem()->code == L::Error::Compile && a->accepted() == old);
        Scene f;
        auto c = game();
        c.operations.pop_back();
        c.operations.pop_back();
        ok(f.h->replace_catalog(c));
        f.unchanged([&] { rejects(f.h->prepare(old->plan(), {f.caster, {}, R::Point{}, f.arch}), R::Error::SchemaMismatch); });
        rejects(f.h->release({}), R::Error::InvalidTicket);
        rejects(f.h->touch({}), R::Error::InvalidEvent);
        check(!L::make_lesson({"fake", "common.disguise@1", "", show(), show(), sense(), sense()}, game()));
    });
    test("pending and active checkpoints outlive original owner source catalog and actual plans", [] {
        R::Checkpoint pending, active;
        R::ActorId a, b, caster;
        R::ObjectId arch;
        R::Ticket old;
        std::weak_ptr<const Plan> weak;
        {
            Scene f;
            caster = f.caster;
            a = f.a;
            b = f.b;
            arch = f.arch;
            auto text = show();
            auto p = plan(text);
            weak = p;
            old = take(f.h->prepare(p, {caster, {}, R::Point{}, arch}));
            take(f.h->release(old));
            f.enter();
            pending = take(f.h->checkpoint());
        }
        check(!weak.expired());
        auto h = take(R::ReferenceHost::create(game()));
        ok(h->restore(pending));
        rejects(h->release(old), R::Error::ForeignTicket);
        take(h->advance_to(100));
        check(take(h->perceive(a, arch)).form == Appearance::StoneWall && take(h->perceive(b, arch)).form == Appearance::OpenArch);
        auto d = plan(sense());
        take(h->release(take(h->prepare(d, {caster, {}, R::Point{100, 0}, arch}))));
        active = take(h->checkpoint());
        h.reset();
        pending = {};
        check(!weak.expired());
        h = take(R::ReferenceHost::create(game()));
        ok(h->restore(active));
        active = {};
        check(take(h->knowledge(a)).witnesses[0] == R::Witness{a, arch, Appearance::OpenArch, true, 100, 4, 1});
        take(h->advance_to(299));
        check(take(h->inspect_perception()).disguises[0].instance == 1);
        take(h->advance_to(300));
        check(weak.expired());
        check(take(h->knowledge(a)).count == 1 && take(h->inspect_perception()).instances_issued == 1);
    });
    test("checkpoint incompatible catalog limits and invalid input preserve destination live ticket", [] {
        Scene source;
        source.enter();
        source.cast(plan(show(true)));
        auto cp = take(source.h->checkpoint());
        Scene destination;
        auto ticket = take(destination.h->prepare(plan(show()), {destination.caster, {}, R::Point{}, destination.arch}));
        destination.unchanged([&] { rejects(destination.h->restore({}), R::Error::InvalidCheckpoint); });
        auto c = game();
        c.operations[3].appearances = 1;
        auto other = take(R::ReferenceHost::create(c));
        auto before = other->snapshot();
        rejects(other->restore(cp), R::Error::IncompatibleCheckpoint);
        check(before == other->snapshot());
        R::HostLimits l;
        l.witnesses = 15;
        other = take(R::ReferenceHost::create(game(), l));
        rejects(other->restore(cp), R::Error::IncompatibleCheckpoint);
        take(destination.h->release(ticket));
        c = game();
        c.operations[3].key = "disabled.key";
        ok(source.h->replace_catalog(c));
        source.unchanged([&] { rejects(source.h->checkpoint(), R::Error::SchemaMismatch); });
    });
    test("whole-world rewind retains logical instances but fresh live authority", [] {
        Scene f;
        f.enter();
        f.cast(plan(show(true)));
        auto armed = take(f.h->checkpoint());
        auto ticket = take(f.h->prepare(plan(sense()), {f.caster, {}, R::Point{100, 0}, f.arch}));
        take(f.h->release(ticket));
        check(take(f.h->knowledge(f.a)).count == 1);
        ok(f.h->restore(armed));
        rejects(f.h->release(ticket), R::Error::ForeignTicket);
        check(take(f.h->knowledge(f.a)).count == 0 && f.view(f.a).form == Appearance::StoneWall);
        f.cast(plan(sense()), {100, 0});
        check(take(f.h->knowledge(f.a)).count == 1);
    });
    test("old spoken disk annulus item and event continuation share current checkpoint", [] {
        Scene f;
        f.move(f.a, {150, 0});
        f.move(f.b, {251, 0});
        auto rune = plan(read("S5.verse"));
        auto item = take(f.h->install(rune, f.caster));
        auto event = take(f.h->admit_touch(7, item, f.a));
        auto burst = plan(read("m5/burst.common"));
        take(f.h->release(take(f.h->prepare(burst, {f.caster, {}, R::Point{}}))));
        f.cast(plan(show(true)));
        f.cast(plan(sense()), {150, 0});
        auto cp = take(f.h->checkpoint());
        ok(f.h->restore(cp));
        check(take(f.h->inspect_perception()).disguises[0].instance == 1 && take(f.h->knowledge(f.a)).count == 1);
        check(f.view(f.a).form == Appearance::OpenArch && f.h->snapshot().actors[f.caster.slot].data.mana == 950);
        rejects(f.h->touch(event), R::Error::ForeignEvent);
        take(f.h->touch(take(f.h->resume_touch(7))));
        check(f.h->snapshot().actors[f.a.slot].data.health == 62);
        take(f.h->advance_to(100));
        check(f.h->snapshot().actors[f.a.slot].data.health == 54 && f.h->snapshot().actors[f.a.slot].slow == R::Slow{750, 300});
        take(f.h->advance_to(300));
        check(f.h->snapshot().actors[f.a.slot].slow == R::Slow{} && f.h->items().items[item.slot].charges == 0);
    });
    test("old observer results remain immutable through edits expiry and owner destruction", [] {
        R::Perceived old;
        R::KnowledgeView known;
        {
            Scene f;
            f.enter();
            f.cast(plan(show(true)));
            old = f.view(f.a);
            f.cast(plan(sense()), {100, 0});
            known = take(f.h->knowledge(f.a));
            ok(f.h->update_object(f.arch, {Appearance::StoneWall, false}));
            take(f.h->advance_to(200));
        }
        check(old == R::Perceived{{1, 1}, {0, 1}, Appearance::StoneWall, 0});
        check(known.count == 1 && known.witnesses[0] == R::Witness{{1, 1}, {0, 1}, Appearance::OpenArch, true, 0, 4, 1});
    });
    test("alternate appearance is a real overlay and actor removal keeps other memberships", [] {
        Scene f;
        f.move(f.a, {0, 0});
        f.move(f.b, {100, 0});
        ok(f.h->update_object(f.arch, {Appearance::StoneWall, false}));
        f.cast(plan(replace(show(true), "stone-wall", "open-arch")));
        check(f.view(f.a).form == Appearance::OpenArch && f.view(f.b).form == Appearance::OpenArch);
        check(take(f.h->inspect_perception()).objects[0].data == R::ObjectSpec{Appearance::StoneWall, false});
        ok(f.h->remove_actor(f.a));
        check(f.view(f.b).form == Appearance::OpenArch);
        auto replacement = take(f.h->add_actor({{0, 0}, 50, 100, 0, 1000, true, true, false}));
        check(take(f.h->perceive(replacement, f.arch)).form == Appearance::StoneWall);
        take(f.h->advance_to(200));
        check(f.view(f.b).form == Appearance::StoneWall);
    });
    test("new current schema and subject facts are rechecked at delayed activation", [] {
        for (unsigned which = 0; which < 4; ++which) {
            Scene f;
            f.cast(plan(show()));
            f.enter();
            auto catalog = game();
            if (which == 0)
                catalog.operations[3].key = "changed.disguise";
            if (which == 1)
                catalog.operations[3].appearances = 1;
            if (which == 2)
                catalog.operations[3].max_duration_ticks = 199;
            if (which == 3)
                catalog.burst = false;
            ok(f.h->replace_catalog(catalog));
            f.unchanged([&] { rejects(f.h->advance_to(100), R::Error::SchemaMismatch); });
            literal_health(f, 990);
            check(f.h->snapshot().pending[0].present);
        }
        Scene f;
        f.enter();
        f.cast(plan(show(true)));
        auto delayed = plan(replace(sense(), "after 0 ms", "after 1000 ms"));
        f.cast(delayed, {100, 0});
        auto catalog = game();
        catalog.operations[4].key = "changed.discern";
        ok(f.h->replace_catalog(catalog));
        f.unchanged([&] { rejects(f.h->advance_to(100), R::Error::SchemaMismatch); });
        check(take(f.h->knowledge(f.a)).count == 0);
    });
    test("accepted subject and aim survive request edits caster movement and physical edits", [] {
        Scene f;
        auto other = take(f.h->add_object({Appearance::StoneWall, false}));
        R::Request request{f.caster, {}, R::Point{}, f.arch};
        auto ticket = take(f.h->prepare(plan(show()), request));
        take(f.h->release(ticket));
        request.aim = R::Point{900, 0};
        request.subject = other;
        f.enter();
        f.move(f.caster, {-999, 999});
        ok(f.h->update_object(f.arch, {Appearance::StoneWall, false}));
        take(f.h->advance_to(100));
        auto p = take(f.h->inspect_perception());
        check(p.disguises[0].subject == f.arch && p.disguises[0].audience[f.a.slot] == f.a);
        f.cast(plan(sense()), {100, 0});
        const auto v = take(f.h->knowledge(f.a));
        check(v.count == 1 && v.witnesses[0].observed_form == Appearance::StoneWall && !v.witnesses[0].observed_passable);
        check(p.objects[other.slot].data == R::ObjectSpec{Appearance::StoneWall, false});
    });
    test("witness capacity resolves complete audience before any addition", [] {
        for (std::size_t cap : {1u, 2u}) {
            R::HostLimits limits;
            limits.witnesses = cap;
            Scene f(limits);
            f.move(f.a, {0, 0});
            f.move(f.b, {100, 0});
            f.cast(plan(show(true)));
            auto d = plan(replace(sense(), "disk 1 mm", "disk 250 mm"));
            if (cap == 1)
                f.unchanged([&] { rejects(f.h->prepare(d, {f.caster, {}, R::Point{}, f.arch}), R::Error::WitnessLimit); });
            else {
                f.cast(d);
                check(take(f.h->knowledge(f.a)).count == 1 && take(f.h->knowledge(f.b)).count == 1);
            }
            check(take(f.h->inspect_perception()).witness_count == (cap == 1 ? 0u : 2u));
        }
    });
    test("all sixteen actor generations fit one audience and one witness transition", [] {
        Scene f;
        f.move(f.a, {0, 0});
        f.move(f.b, {100, 0});
        auto caster = f.h->snapshot().actors[f.caster.slot].data;
        caster.position = {0, 0};
        caster.eligible = true;
        ok(f.h->update_actor(f.caster, caster));
        for (std::size_t i = 3; i < 16; ++i)
            take(f.h->add_actor({{0, 0}, 50, 100, 0, 1000, true, true, false}));
        auto show_report = f.cast(plan(show(true)));
        check(show_report->usage.selected_targets == 16);
        auto d = plan(replace(sense(), "disk 1 mm", "disk 250 mm"));
        auto report = f.cast(d);
        check(report->usage.selected_targets == 16 && take(f.h->inspect_perception()).witness_count == 16);
        check(take(f.h->knowledge(f.caster)).count == 1);
    });
    test("discernment at exact expiry refuses and keeps the previously paid timeline", [] {
        Scene f;
        f.enter();
        f.cast(plan(show(true)));
        auto d = plan(replace(sense(), "after 0 ms", "after 2000 ms"));
        f.cast(d, {100, 0});
        f.unchanged([&] { rejects(f.h->advance_to(200), R::Error::NoDisguise); });
        literal_health(f, 980);
        check(f.h->snapshot().tick == 0 && f.view(f.a).form == Appearance::StoneWall);
        take(f.h->cancel(2));
        take(f.h->advance_to(200));
        check(f.view(f.a).form == Appearance::OpenArch);
    });
    std::cout << passed << " perception contract cases passed; " << failed << " failed.\n";
    return failed ? 1 : 0;
}
