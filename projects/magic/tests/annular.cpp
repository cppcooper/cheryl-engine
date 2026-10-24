#include <magic/learning.h>
#include <magic/reference-host.h>
#include "literals.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
    using namespace Magic;
    namespace R = Magic::Reference;
    namespace L = Magic::Learning;
    int passed = 0, failures = 0;
    void check(bool b, std::string_view why) {
        if (!b)
            throw std::runtime_error(std::string(why));
    }
    template <class T, class E> T take(std::expected<T, E> r) {
        if (!r)
            throw std::runtime_error(std::string(error_name(r.error().code)));
        return std::move(*r);
    }
    template <class E> void ok(std::expected<void, E> r) {
        if (!r)
            throw std::runtime_error(std::string(error_name(r.error().code)));
    }
    template <class T, class E, class C> void rejects(const std::expected<T, E>& r, C c) {
        check(!r, "expected refusal");
        if (r.error().code != c)
            throw std::runtime_error("wrong refusal code");
    }
    void test(std::string_view name, const std::function<void()>& body) {
        try {
            body();
            ++passed;
            std::cout << "PASS " << name << '\n';
        } catch (const std::exception& e) {
            ++failures;
            std::cout << "FAIL " << name << ": " << e.what() << '\n';
        }
    }
    Catalog game() {
        Catalog c;
        c.operations = {{Primitive::Heal, "game.heal"}, {Primitive::Fire, "game.fire"}, {Primitive::Slow, "game.slow"}};
        return c;
    }
    std::string read(std::string_view file) {
        std::ifstream in(std::string(MAGIC_FIXTURE_DIR) + "/" + std::string(file));
        check(in.good(), "fixture opens");
        std::string s(std::istreambuf_iterator<char>(in), {});
        if (s.ends_with('\n'))
            s.pop_back();
        return s;
    }
    PlanHandle plan(std::string_view s, const Options& o = {}) {
        auto p = Magic::compile(s, game(), o);
        if (!p)
            throw std::runtime_error(p.error().message);
        return *p;
    }
    std::string replace(std::string s, std::string_view a, std::string_view b) {
        auto pos = s.find(a);
        check(pos != s.npos, "authored replacement exists");
        s.replace(pos, a.size(), b);
        return s;
    }
    const std::string annular_shape = "annulus excluding inner disk 100 mm inclusive within outer disk 250 mm inclusive";
    std::string source(bool immediate = false) {
        auto s = read("m5/burst.common");
        return immediate ? replace(s, "after 1000 ms", "after 0 ms") : s;
    }
    std::string disk(bool immediate = false) {
        return replace(source(immediate), annular_shape, "disk 250 mm");
    }
    // Disk syntax puts its inclusive marker at the end, unlike the explicit annulus boundaries.
    std::string disk_source(bool immediate = false) {
        return replace(disk(immediate), "queried at activation ;", "queried at activation inclusive ;");
    }
    const std::array<R::Point, 5> points = {R::Point{0, 0}, {100, 0}, {101, 0}, {150, 200}, {251, 0}};
    R::ActorSpec caster_spec() {
        return {{600, 0}, 100, 100, 100, 1000, false, true, true};
    }
    R::ActorSpec target_spec(R::Point p) {
        return {p, 50, 100, 0, 1000, true, true, false};
    }
    struct Scene {
        std::unique_ptr<R::ReferenceHost> h;
        R::ActorId caster;
        std::array<R::ActorId, 5> actors;
        explicit Scene(R::HostLimits l = {}, bool reverse = false) {
            h = take(R::ReferenceHost::create(game(), l));
            caster = take(h->add_actor(caster_spec()));
            for (std::size_t n = 0; n < 5; ++n) {
                auto i = reverse ? 4 - n : n;
                actors[i] = take(h->add_actor(target_spec(points[i])));
            }
        }
        R::Observed cast(const PlanHandle& p, R::Point aim = {}) { return take(h->release(take(h->prepare(p, {caster, {}, aim})))); }
        template <class F> void unchanged(F body) {
            auto w = h->snapshot();
            auto i = h->items();
            body();
            check(h->snapshot() == w && h->items() == i, "full state/resources/queue/ledger preserved");
        }
    };
    R::Snapshot literal_before(const Scene& f) {
        R::Snapshot w;
        w.revision = 6;
        for (std::size_t i = 0; i < R::actor_capacity; ++i)
            w.actors[i].id.slot = static_cast<std::uint32_t>(i);
        w.actors[0] = {{0, 1}, true, {{600, 0}, 100, 100, 100, 1000, false, true, true}, {1000, 0}};
        for (std::size_t role = 0; role < 5; ++role)
            w.actors[f.actors[role].slot] = {f.actors[role], true, {points[role], 50, 100, 0, 1000, true, true, false}, {1000, 0}};
        return w;
    }
    Meaning literal_meaning() {
        Meaning m;
        m.delivery = Delivery::Burst;
        m.onset_ticks = 100;
        m.bindings = {{"near", Query::EligibleAnnulus, "G0", 250, BindingTime::Release, BindingTime::Activation, true, 100}};
        m.effects = {{Primitive::Fire, "game.fire", 0, 8, 0}, {Primitive::Slow, "game.slow", 0, 750, 200}};
        return m;
    }
    struct Teaching {
        std::string original = read("m5/lesson.verse"), gloss = read("m5/lesson.common"), practice = read("m5/practice.verse"),
                    practice_gloss = read("m5/practice.common");
        L::LessonInput input() const {
            return {"protected-centre",
                    "target.annulus@1",
                    "Invented Mend/Cinder; target bridge excludes its inclusive inner disk.",
                    original,
                    gloss,
                    practice,
                    practice_gloss};
        }
        L::LessonHandle make(L::Limits l = {}) const { return take(L::make_lesson(input(), game(), l)); }
    };
    void diagnostic(std::string s, Code code) {
        auto result = Magic::compile(s, game());
        check(!result, "compile refuses invalid shape");
        auto d = result.error();
        check(
            d.code == code && d.span.offset <= s.size() && d.span.length <= s.size() - d.span.offset && !d.message.empty(),
            "owned span-aware diagnostic"
        );
        s.clear();
        check(!d.message.empty(), "diagnostic owns lifetime");
    }
    std::string many_bindings(std::size_t n) {
        std::string s = "spoken release via burst after 1000 ms { ";
        for (std::size_t i = 0; i < n; ++i)
            s += "bind b" + std::to_string(i) + " = eligible actors in G0 " + annular_shape +
                 " around aim captured at release queried at activation ; ";
        return s + "together ordered { on b0 bridge Common@1 { heal 1 hp instant } ; } }";
    }
    struct Tiny {
        std::unique_ptr<R::ReferenceHost> h;
        R::ActorId c, t;
        explicit Tiny(R::HostLimits l = {}) {
            h = take(R::ReferenceHost::create(game(), l));
            c = take(h->add_actor(caster_spec()));
            t = take(h->add_actor(target_spec({150, 0})));
        }
        void queue(const PlanHandle& p, R::Point aim = {}) { take(h->release(take(h->prepare(p, {c, {}, aim})))); }
    };
}
int main() {
    test("independent complete annular meaning Common and school bridge equivalence", [] {
        auto p = plan(read("m5/burst.verse"));
        check(p->meaning() == literal_meaning(), "literal full meaning");
        check(p->common() == read("m5/burst.common"), "independent exact Common");
        check(plan(p->common())->meaning() == literal_meaning(), "Common reparses equivalent meaning");
        check(p->requirements() & rule_bit(Rule::AnnularTarget), "actual recognized modifier requirement");
        check(
            p->binding_provenance().size() == 1 && p->binding_provenance()[0].rule == Rule::AnnularTarget,
            "bounded checked binding provenance"
        );
        check(p->inspection().find("inner_radius < distance") != std::string_view::npos, "inspection exposes exact inner/outer inequality");
    });
    test("literal geometry in both actor insertion orders", [] {
        for (bool reverse : {false, true}) {
            Scene f({}, reverse);
            check(f.h->snapshot() == literal_before(f), "literal initial world");
            auto p = plan(source(true));
            auto report = f.cast(p);
            auto expected = literal_before(f);
            expected.revision = 7;
            expected.actions_issued = 1;
            expected.actors[0].data.mana = 80;
            expected.actors[f.actors[2].slot].data.health = 42;
            expected.actors[f.actors[2].slot].slow = {750, 200};
            expected.actors[f.actors[3].slot].data.health = 42;
            expected.actors[f.actors[3].slot].slow = {750, 200};
            check(
                report->before == literal_before(f) && report->after == expected,
                "centre/inner excluded,101 and150/200 included,251 excluded"
            );
            check(
                report->usage.selector_work == 32 && report->usage.selected_targets == 2,
                "one shared binding charged twice per slot, not per effect"
            );
            take(f.h->advance_to(200));
            expected.tick = 200;
            expected.revision = 8;
            expected.actors[f.actors[2].slot].slow = {};
            expected.actors[f.actors[3].slot].slow = {};
            check(f.h->snapshot() == expected, "literal expiry");
            check(report->after.actors[f.actors[2].slot].slow == R::Slow{750, 200}, "old report immutable");
        }
    });
    test("zero inner excludes exact centre while old disk includes it", [] {
        Scene ring;
        auto p = plan(replace(source(true), "inner disk 100 mm", "inner disk 0 mm"));
        ring.cast(p);
        check(
            ring.h->snapshot().actors[ring.actors[0].slot].data.health == 50 &&
                ring.h->snapshot().actors[ring.actors[1].slot].data.health == 42,
            "zero excludes only centre of included inner disk"
        );
        Scene old;
        auto disk_plan = plan(disk_source(true));
        check(
            disk_plan->meaning().bindings[0].query == Query::EligibleDisk && disk_plan->meaning().bindings[0].inner_radius_mm == 0,
            "distinct old disk tag/default"
        );
        old.cast(disk_plan);
        check(
            old.h->snapshot().actors[old.actors[0].slot].data.health == 42 &&
                old.h->snapshot().actors[old.actors[0].slot].slow == R::Slow{750, 200},
            "ordinary disk centre unchanged behavior"
        );
    });
    test("literal delayed release onset and exact duration", [] {
        Scene f;
        auto p = plan(source());
        auto release = f.cast(p);
        auto expected = literal_before(f);
        expected.revision = 7;
        expected.actions_issued = 1;
        expected.actors[0].data.mana = 80;
        expected.pending[0] = {true, 1, 100, f.caster, {0, 0}, p->metrics().retained_bytes};
        check(release->after == expected, "literal captured queue release");
        take(f.h->advance_to(99));
        expected.tick = 99;
        expected.revision = 8;
        check(f.h->snapshot() == expected, "no early effects");
        take(f.h->advance_to(100));
        expected.tick = 100;
        expected.revision = 9;
        expected.pending[0] = {};
        for (auto role : {2, 3}) {
            expected.actors[f.actors[role].slot].data.health = 42;
            expected.actors[f.actors[role].slot].slow = {750, 300};
        }
        check(f.h->snapshot() == expected, "literal shared activation");
        take(f.h->advance_to(299));
        expected.tick = 299;
        expected.revision = 10;
        check(f.h->snapshot() == expected, "before expiry");
        take(f.h->advance_to(300));
        expected.tick = 300;
        expected.revision = 11;
        for (auto role : {2, 3})
            expected.actors[f.actors[role].slot].slow = {};
        check(f.h->snapshot() == expected, "exact expiry200 from activation");
    });
    test("activation queries moved removed ineligible and dead current centres", [] {
        for (bool reverse : {false, true}) {
            Scene f({}, reverse);
            auto removed = take(f.h->add_actor(target_spec({120, 0})));
            auto ineligible = take(f.h->add_actor(target_spec({200, 0})));
            auto dead = take(f.h->add_actor(target_spec({200, 0})));
            f.cast(plan(source()));
            auto move = [&](std::size_t role, R::Point pos) {
                auto d = target_spec(pos);
                ok(f.h->update_actor(f.actors[role], d));
            };
            move(0, {101, 0});
            move(2, {100, 0});
            move(3, {251, 0});
            move(4, {150, 200});
            ok(f.h->remove_actor(removed));
            auto d = target_spec({200, 0});
            d.eligible = false;
            ok(f.h->update_actor(ineligible, d));
            d.eligible = true;
            d.health = 0;
            ok(f.h->update_actor(dead, d));
            take(f.h->advance_to(100));
            auto w = f.h->snapshot();
            for (auto role : {0, 4})
                check(
                    w.actors[f.actors[role].slot].data.health == 42 && w.actors[f.actors[role].slot].slow == R::Slow{750, 300},
                    "current entrant selected"
                );
            for (auto role : {1, 2, 3})
                check(
                    w.actors[f.actors[role].slot].data.health == 50 && w.actors[f.actors[role].slot].slow == R::Slow{},
                    "departed members protected"
                );
            check(
                !w.actors[removed.slot].present && w.actors[ineligible.slot].data.health == 50 && w.actors[dead.slot].slow == R::Slow{},
                "removed/ineligible/dead not selected"
            );
        }
    });
    test("both contextual pixel radii freeze independently of changed authoring camera", [] {
        auto s = replace(replace(source(), "inner disk 100 mm", "inner disk 10 px"), "outer disk 250 mm", "outer disk 25 px");
        Options o;
        o.pixels = PixelContext{"G0", "C7", 3, "ground", 10, 1, true};
        auto p = plan(s, o);
        check(p->meaning() == literal_meaning() && p->common() == read("m5/burst.common"), "positive contextual radii equal100/250mm");
        Scene f;
        R::Request request{f.caster, {}, R::Point{0, 0}};
        take(f.h->release(take(f.h->prepare(p, request))));
        o.pixels->camera = "C8";
        o.pixels->camera_revision = 4;
        o.pixels->mm_numerator = 20;
        request.aim = R::Point{9000, 9000};
        check(plan(s, o)->meaning().bindings[0].inner_radius_mm == 200, "new camera can change a new authored plan");
        check(
            p->meaning() == literal_meaning() && f.h->snapshot().pending[0].aim == R::Point{0, 0} &&
                p->conversion().find("C7 revision 3") != std::string_view::npos,
            "accepted plan/point keep old context"
        );
        take(f.h->advance_to(100));
        check(f.h->snapshot().actors[f.actors[2].slot].data.health == 42, "frozen world radius still hits101");
        rejects(Magic::compile(s, game()), Code::MissingContext);
    });
    test("inner equal greater negative inexact dimensions unsupported units and overflow refuse", [] {
        diagnostic(replace(source(), "inner disk 100 mm", "inner disk 250 mm"), Code::Range);
        diagnostic(replace(source(), "inner disk 100 mm", "inner disk 251 mm"), Code::Range);
        diagnostic(replace(source(), "inner disk 100 mm", "inner disk -1 mm"), Code::Range);
        diagnostic(replace(source(), "inner disk 100 mm", "inner disk 0.1 mm"), Code::Inexact);
        diagnostic(replace(source(), "inner disk 100 mm", "inner disk 1 s"), Code::WrongDimension);
        diagnostic(replace(source(), "outer disk 250 mm", "outer disk 250 hp"), Code::WrongDimension);
        diagnostic(replace(source(), "inner disk 100 mm", "inner disk 100 cubits"), Code::UnsupportedUnit);
        diagnostic(replace(source(), "outer disk 250 mm", "outer disk 1000001 mm"), Code::Range);
        diagnostic(replace(source(), "outer disk 250 mm", "outer disk 18446744073709551616 mm"), Code::Range);
    });
    test("unknown and ambiguous shape scope has owned diagnostics", [] {
        diagnostic(replace(source(), "annulus excluding", "torus excluding"), Code::UnknownPhrase);
        diagnostic(replace(disk_source(), "disk 250 mm around", "disk 250 mm except 100 mm around"), Code::AmbiguousScope);
        diagnostic(replace(source(), "inner disk 100 mm inclusive", "inner disk 100 mm exclusive"), Code::Malformed);
        diagnostic(replace(source(), "queried at activation ;", "queried at activation excluding 100 mm ;"), Code::AmbiguousScope);
    });
    test("max normalized radii and extreme checked distance arithmetic", [] {
        auto p =
            plan(replace(replace(source(true), "inner disk 100 mm", "inner disk 999999 mm"), "outer disk 250 mm", "outer disk 1000000 mm"));
        auto h = take(R::ReferenceHost::create(game()));
        auto c = take(h->add_actor(caster_spec()));
        auto edge = take(h->add_actor(target_spec({0, -1000000})));
        auto inner = take(h->add_actor(target_spec({-1, -1000000})));
        auto far = take(h->add_actor(target_spec({1000000, 1000000})));
        take(h->release(take(h->prepare(p, {c, {}, R::Point{-1000000, -1000000}}))));
        auto w = h->snapshot();
        check(
            w.actors[edge.slot].data.health == 42 && w.actors[inner.slot].data.health == 50 && w.actors[far.slot].data.health == 50,
            "1e6 outer included,999999 inner excluded,2e6 deltas safe"
        );
    });
    test("compiler and current host can explicitly disable new query", [] {
        auto c = game();
        c.annular = false;
        rejects(Magic::compile(source(), c), Code::UnsupportedCombination);
        Scene f;
        ok(f.h->replace_catalog(c));
        f.unchanged([&] { rejects(f.h->prepare(plan(source()), {f.caster, {}, R::Point{0, 0}}), R::Error::UnsupportedBinding); });
        auto foreign = plan(replace(source(), "in G0 annulus", "in G1 annulus"));
        Scene other;
        other.unchanged([&] { rejects(other.h->prepare(foreign, {other.caster, {}, R::Point{0, 0}}), R::Error::UnsupportedBinding); });
    });
    test("disk annulus overlapping field conflicts in both binding/effect/storage orders", [] {
        for (bool reverse : {false, true})
            for (bool effects_reverse : {false, true}) {
                Scene f({}, reverse);
                std::string s =
                    "spoken release via burst after 0 ms { bind a = eligible actors in G0 disk 250 mm around aim captured at release queried at activation inclusive ; bind b = eligible actors in G0 " +
                    annular_shape + " around aim captured at release queried at activation ; together ordered { ";
                const std::string heal = "on a bridge Common@1 { heal 9 hp instant } ; ",
                                  fire = "on b bridge Common@1 { fire damage 8 dp instant } ; ";
                s += effects_reverse ? fire + heal : heal + fire;
                s += "} }";
                f.unchanged([&] { rejects(f.h->prepare(plan(s), {f.caster, {}, R::Point{0, 0}}), R::Error::Overlap); });
            }
    });
    test("active slow and status limits cannot be evaded by annular selection", [] {
        Scene f;
        auto old = f.cast(plan(read("S3.verse")));
        f.unchanged([&] { rejects(f.h->prepare(plan(source(true)), {f.caster, {}, R::Point{0, 0}}), R::Error::ActiveSlow); });
        check(old->after.actors[f.actors[2].slot].slow == R::Slow{750, 200}, "prior output usable");
        R::HostLimits l;
        l.statuses = 1;
        Scene limited(l);
        limited.unchanged([&] {
            rejects(limited.h->prepare(plan(source(true)), {limited.caster, {}, R::Point{0, 0}}), R::Error::StatusLimit);
        });
    });
    test("empty annular selection and whole-step later failure preserve all accepted work", [] {
        Tiny f;
        auto p = plan(source());
        f.queue(p);
        f.queue(p, {9000, 9000});
        auto before = f.h->snapshot();
        auto bad = f.h->advance_to(100);
        rejects(bad, R::Error::EmptySelection);
        check(bad.error().action == 2 && f.h->snapshot() == before, "earlier scratch fire/slow and clock queue rolled back");
        take(f.h->cancel(2));
        take(f.h->advance_to(100));
        check(
            f.h->snapshot().actors[f.t.slot].data.health == 42 && f.h->snapshot().actors[f.c.slot].data.mana == 60,
            "explicit cancel no refund, first action executes once"
        );
        auto h = take(R::ReferenceHost::create(game()));
        auto c = take(h->add_actor(caster_spec()));
        take(h->add_actor(target_spec({0, 0})));
        auto w = h->snapshot();
        rejects(h->prepare(plan(source(true)), {c, {}, R::Point{0, 0}}), R::Error::EmptySelection);
        check(h->snapshot() == w, "centre-only query no effects or debit");
    });
    test("explicit selector32 and work exact versus one-short without raising caps", [] {
        auto p = plan(source());
        Tiny probe;
        probe.queue(p);
        auto usage = take(probe.h->advance_to(100))->usage;
        Tiny old;
        old.queue(plan(disk_source()));
        auto old_usage = take(old.h->advance_to(100))->usage;
        check(
            usage.selector_work == 32 && old_usage.selector_work == 16 && usage.work == old_usage.work + 18,
            "two radius units plus16 extra inner comparisons"
        );
        for (bool short_work : {false, true}) {
            R::HostLimits l;
            l.work = usage.work - (short_work ? 1 : 0);
            Tiny f(l);
            f.queue(p);
            auto w = f.h->snapshot();
            if (short_work) {
                rejects(f.h->advance_to(100), R::Error::WorkLimit);
                check(f.h->snapshot() == w, "failed work preserves paid queue/world");
            } else
                take(f.h->advance_to(100));
        }
        R::HostLimits l;
        l.selector_work = 31;
        Tiny short_selector(l);
        short_selector.queue(p);
        auto w = short_selector.h->snapshot();
        rejects(short_selector.h->advance_to(100), R::Error::SelectorLimit);
        check(short_selector.h->snapshot() == w, "selector refusal staged");
        l.selector_work = 32;
        Tiny exact(l);
        exact.queue(p);
        take(exact.h->advance_to(100));
        std::cout << "annular activation work=" << usage.work << " disk=" << old_usage.work << " selector=" << usage.selector_work << '\n';
    });
    test("complete-step selector ceiling1024 exact and1023 rollback", [] {
        auto p = plan(many_bindings(4));
        for (bool short_limit : {false, true}) {
            R::HostLimits l;
            l.selector_work = short_limit ? 1023 : 1024;
            Tiny f(l);
            for (int i = 0; i < 8; ++i)
                f.queue(p);
            auto w = f.h->snapshot();
            auto result = f.h->advance_to(100);
            if (short_limit) {
                rejects(result, R::Error::SelectorLimit);
                check(f.h->snapshot() == w, "whole eight-action step rollback");
            } else {
                auto report = take(std::move(result));
                check(
                    report->usage.selector_work == 1024 && report->usage.selected_targets == 32 &&
                        report->after.actors[f.t.slot].data.health == 58,
                    "exact total step units, unused bindings still resolved"
                );
            }
        }
    });
    test("selected target ceiling128 exact and127 rollback", [] {
        auto p = plan(many_bindings(8));
        for (bool short_limit : {false, true}) {
            R::HostLimits l;
            l.selected_targets = short_limit ? 127 : 128;
            auto h = take(R::ReferenceHost::create(game(), l));
            auto cdata = caster_spec();
            cdata.position = {150, 0};
            cdata.eligible = true;
            auto c = take(h->add_actor(cdata));
            for (int i = 0; i < 15; ++i)
                take(h->add_actor(target_spec({150, 0})));
            take(h->release(take(h->prepare(p, {c, {}, R::Point{0, 0}}))));
            auto w = h->snapshot();
            auto result = h->advance_to(100);
            if (short_limit) {
                rejects(result, R::Error::TargetLimit);
                check(h->snapshot() == w, "selected quota rollback");
            } else
                check(take(std::move(result))->usage.selected_targets == 128, "exact128 binding/actor pairs");
        }
    });
    test("new provenance text tokens work storage exact compiler budgets", [] {
        auto text = source();
        auto p = plan(text);
        auto m = p->metrics();
        Options o;
        o.limits.tokens = m.tokens;
        o.limits.work = m.work;
        o.limits.retained_bytes = m.retained_bytes;
        check(plan(text, o)->meaning() == literal_meaning(), "exact admitted metadata/source budgets");
        --o.limits.tokens;
        rejects(Magic::compile(text, game(), o), Code::TokenLimit);
        o.limits.tokens = m.tokens;
        --o.limits.work;
        rejects(Magic::compile(text, game(), o), Code::WorkLimit);
        o.limits.work = m.work;
        --o.limits.retained_bytes;
        rejects(Magic::compile(text, game(), o), Code::StorageLimit);
        Options size;
        size.limits.source_bytes = text.size();
        plan(text, size);
        --size.limits.source_bytes;
        rejects(Magic::compile(text, game(), size), Code::SourceLimit);
        std::cout << "annular compiler tokens=" << m.tokens << " work=" << m.work << " retained=" << m.retained_bytes << '\n';
    });
    test("targeting lesson derives owned exact target alignment and teaches only ninth bit", [] {
        auto lesson = Teaching{}.make();
        check(
            lesson->target_alignments().size() == 1 && lesson->target_alignments()[0].production == Rule::AnnularTarget,
            "recognized target alignment"
        );
        const std::string expected =
            "bind near = eligible actors in G0 " + annular_shape + " around aim captured at release queried at activation ;";
        check(
            lesson->target_alignments()[0].original == expected && lesson->target_alignments()[0].gloss == expected,
            "independent target declaration literal"
        );
        auto learner = take(L::Learner::create(rule_bit(Rule::TargetBridge)));
        check(take(learner.learn(lesson)), "modifier learns with bridge prerequisite");
        check(
            learner.snapshot().known == (rule_bit(Rule::TargetBridge) | rule_bit(Rule::AnnularTarget)),
            "does not grant heal fire slow or school operations"
        );
        check(!take(learner.learn(lesson)), "idempotent");
        auto absent = take(L::Learner::create(0));
        rejects(absent.learn(lesson), L::Error::Prerequisite);
    });
    test("annular lesson work retained exact and one-short, false claims refuse", [] {
        Teaching t;
        auto lesson = t.make();
        auto m = lesson->usage();
        L::Limits work;
        work.work = m.work;
        t.make(work);
        --work.work;
        rejects(L::make_lesson(t.input(), game(), work), L::Error::WorkLimit);
        L::Limits storage;
        storage.retained_bytes = m.retained_bytes;
        t.make(storage);
        --storage.retained_bytes;
        rejects(L::make_lesson(t.input(), game(), storage), L::Error::StorageLimit);
        t.original = read("S2.verse");
        t.gloss = std::string(Literal::S2);
        rejects(L::make_lesson(t.input(), game()), L::Error::ContradictoryLesson);
        std::cout << "annular lesson work=" << m.work << " retained=" << m.retained_bytes << '\n';
    });
    test("full eight target alignments and four effect rows remain bounded", [] {
        auto a = many_bindings(8);
        a = replace(
            a, "on b0 bridge Common@1 { heal 1 hp instant } ;",
            "on b0 bridge Common@1 { heal 1 hp instant } ; on b1 bridge Common@1 { heal 2 hp instant } ; on b2 bridge Common@1 { heal 3 hp instant } ; on b3 bridge Common@1 { heal 4 hp instant } ;"
        );
        for (unsigned i = 1; i < 4; ++i) {
            const auto prefix = "bind b" + std::to_string(i) + " = eligible actors in G0 ";
            const auto shape = "annulus excluding inner disk " + std::to_string(200 + 50 * i) + " mm inclusive within outer disk " +
                               std::to_string(250 + 50 * i) + " mm inclusive";
            a = replace(a, prefix + annular_shape, prefix + shape);
        }
        auto b = replace(a, "inner disk 100 mm", "inner disk 50 mm");
        auto pa = plan(a), pb = plan(b);
        auto lesson = take(
            L::make_lesson(
                {"eight-targets", "target.annulus@1", "Invented Common-compatible targeting.", a, pa->common(), b, pb->common()}, game()
            )
        );
        check(
            lesson->target_alignments().size() == 8 && lesson->aligned().size() == 4,
            "eight target/four effect rows within existing ceilings"
        );
        auto host = take(R::ReferenceHost::create(game()));
        auto caster = take(host->add_actor(caster_spec()));
        std::array<R::ActorId, 4> recipients;
        for (std::size_t i = 0; i < 4; ++i)
            recipients[i] = take(host->add_actor(target_spec({i == 0 ? 150 : static_cast<std::int32_t>(225 + 50 * i), 0})));
        take(host->release(take(host->prepare(lesson->worked(), {caster, {}, R::Point{0, 0}}))));
        take(host->advance_to(100));
        for (std::size_t i = 0; i < 4; ++i)
            check(
                host->snapshot().actors[recipients[i].slot].data.health == 51 + i,
                "four disjoint bands execute independent literal healing1..4"
            );
        check(host->snapshot().actors[caster.slot].data.mana == 60, "four effects pay40 once");
        rejects(Magic::compile(many_bindings(9), game()), Code::BindingLimit);
    });
    test("five bounded lesson receipts and knowledge revision boundary coexist", [] {
        std::array<L::LessonHandle, 4> old;
        auto make = [](std::string_view id, std::string_view rule, const std::string& a, const std::string& ca, const std::string& b,
                       const std::string& cb) {
            return take(L::make_lesson({id, rule, "Invented school production.", a, ca, b, cb}, game()));
        };
        const auto heal = read("S1.verse"), heal_gloss = std::string(Literal::S1);
        old[0] = make("mend-heal", "mend.heal@1", heal, heal_gloss, replace(heal, "12 hp", "13 hp"), replace(heal_gloss, "12 hp", "13 hp"));
        const auto slow = read("S4.verse"), slow_gloss = std::string(Literal::S2);
        old[1] = make("mend-slow", "mend.slow@1", slow, slow_gloss, replace(slow, "750", "700"), replace(slow_gloss, "750", "700"));
        old[2] = make("cinder-fire", "cinder.fire@1", slow, slow_gloss, replace(slow, "8 dp", "9 dp"), replace(slow_gloss, "8 dp", "9 dp"));
        old[3] = make(
            "cinder-slow", "cinder.slow@1", read("m4/worked.verse"), read("m4/worked.common"), read("m4/practice.verse"),
            read("m4/practice.common")
        );
        const RuleMask base =
            rule_bit(Rule::CommonHeal) | rule_bit(Rule::CommonFire) | rule_bit(Rule::CommonSlow) | rule_bit(Rule::TargetBridge);
        auto target = Teaching{}.make();
        for (std::uint64_t revision_cap : {4, 5}) {
            auto learner = take(L::Learner::create(base, revision_cap));
            for (const auto& lesson : old)
                check(take(learner.learn(lesson)), "new school production");
            check(learner.snapshot().known == 255 && learner.snapshot().revision == 4, "literal original eight-bit subset");
            if (revision_cap == 4) {
                auto before = learner.snapshot();
                rejects(learner.learn(target), L::Error::RevisionLimit);
                check(learner.snapshot() == before, "fifth lesson preserves exhausted knowledge");
            } else {
                check(
                    take(learner.learn(target)) && learner.snapshot().known == 511 && learner.snapshot().revision == 5,
                    "all nine rules, five retained lesson definitions"
                );
                for (const auto& lesson : old)
                    check(!take(learner.learn(lesson)), "old definition remains idempotent");
                Teaching conflict;
                auto input = conflict.input();
                input.identity = "mend-heal";
                rejects(learner.learn(take(L::make_lesson(input, game()))), L::Error::DuplicateIdentity);
            }
        }
        rejects(L::Learner::create(static_cast<RuleMask>(all_rules + 1)), L::Error::InvalidLimits);
    });
    test("book lesson edit preserves prior accepted disk and malformed annular draft", [] {
        auto l = take(L::Learner::create(all_rules & ~rule_bit(Rule::AnnularTarget)));
        auto b = take(L::Spellbook::create());
        ok(b->add("burst", "Protected centre"));
        auto old = take(b->edit("burst", read("S2.verse"), "", game(), l))->accepted();
        auto unknown = take(b->edit("burst", read("m5/burst.verse"), "", game(), l));
        check(
            unknown->problem() && unknown->problem()->missing == rule_bit(Rule::AnnularTarget) && unknown->accepted() == old,
            "recognized modifier not yet available"
        );
        take(l.learn(Teaching{}.make()));
        auto changed = take(b->edit("burst", read("m5/burst.verse"), "", game(), l));
        check(
            !changed->problem() && changed->accepted()->revision() == 2 && old->plan()->meaning().bindings[0].query == Query::EligibleDisk,
            "accepted new tag, old immutable tag"
        );
        auto bad = take(b->edit("burst", replace(source(), "inner disk 100 mm", "inner disk 250 mm"), "", game(), l));
        check(
            bad->problem()->diagnostic->code == Code::Range && bad->accepted() == changed->accepted(),
            "invalid bound retains current acceptance"
        );
        auto c = game();
        c.annular = false;
        Scene f;
        ok(f.h->replace_catalog(c));
        f.unchanged([&] {
            rejects(f.h->prepare(changed->accepted()->plan(), {f.caster, {}, R::Point{0, 0}}), R::Error::UnsupportedBinding);
            rejects(f.h->release(R::Ticket{}), R::Error::InvalidTicket);
            rejects(f.h->touch(R::TouchEvent{}), R::Error::InvalidEvent);
        });
    });
    test("pending annulus checkpoint owns plan point time and M3 event ledger across destruction", [] {
        R::Checkpoint cp;
        std::weak_ptr<const Plan> weak;
        R::Ticket old_ticket;
        R::TouchEvent old_event;
        R::ItemId item;
        R::ActorId target;
        {
            auto c = game();
            auto text = source();
            auto p = plan(text);
            weak = p;
            auto h = take(R::ReferenceHost::create(c));
            auto caster = take(h->add_actor(caster_spec()));
            target = take(h->add_actor(target_spec({1150, -500})));
            item = take(h->install(plan(read("S5.verse")), caster));
            old_event = take(h->admit_touch(17, item, target));
            old_ticket = take(h->prepare(p, {caster, {}, R::Point{1000, -500}}));
            take(h->release(old_ticket));
            take(h->advance_to(99));
            cp = take(h->checkpoint());
            text.clear();
            c.operations.clear();
        }
        check(!weak.expired(), "actual immutable retained pending plan");
        auto h = take(R::ReferenceHost::create(game()));
        ok(h->restore(cp));
        check(
            h->snapshot().tick == 99 && h->snapshot().pending[0].due_tick == 100 && h->snapshot().pending[0].aim == R::Point{1000, -500},
            "retained exact timing/aim"
        );
        auto before = h->snapshot();
        auto items = h->items();
        rejects(h->release(old_ticket), R::Error::ForeignTicket);
        rejects(h->touch(old_event), R::Error::ForeignEvent);
        check(h->snapshot() == before && h->items() == items, "old live authority never revives");
        take(h->touch(take(h->resume_touch(17))));
        cp = R::Checkpoint{};
        take(h->advance_to(100));
        check(weak.expired(), "retained plan released when activation completes");
        check(
            h->snapshot().actors[target.slot].data.health == 54 && h->snapshot().actors[target.slot].slow == R::Slow{750, 300} &&
                h->items().items[item.slot].charges == 0 && h->items().ledger[0].consumed,
            "old event heal12 and annular fire8/slow share restored world"
        );
    });
    test("checkpoint unsupported current query or incompatible schema refuses before adoption", [] {
        Tiny source_host;
        source_host.queue(plan(source()));
        auto cp = take(source_host.h->checkpoint());
        auto disabled = game();
        disabled.annular = false;
        ok(source_host.h->replace_catalog(disabled));
        auto w = source_host.h->snapshot();
        rejects(source_host.h->checkpoint(), R::Error::UnsupportedBinding);
        check(source_host.h->snapshot() == w, "bad retained query capture refuses");
        auto h = take(R::ReferenceHost::create(disabled));
        auto c = take(h->add_actor(caster_spec()));
        auto t = take(h->add_actor(target_spec({0, 0})));
        auto ticket = take(h->prepare(plan(disk_source()), {c, {}, R::Point{0, 0}}));
        auto before = h->snapshot();
        rejects(h->restore(cp), R::Error::IncompatibleCheckpoint);
        check(h->snapshot() == before, "annular schema mismatch cannot downgrade to disk");
        take(h->release(ticket));
        take(h->advance_to(100));
        check(h->snapshot().actors[t.slot].data.health == 42, "previous output/ticket still usable");
        auto compatible = take(R::ReferenceHost::create(game()));
        ok(compatible->restore(cp));
        take(compatible->advance_to(100));
        check(compatible->snapshot().actors[source_host.t.slot].data.health == 42, "refused restore did not damage checkpoint");
    });
    test("old disk/item checkpoint remains meaningful after new lesson and book edits", [] {
        R::Checkpoint cp;
        L::EntryHandle old;
        R::ActorId centre;
        R::ItemId rune;
        {
            auto learner = take(L::Learner::create(all_rules & ~rule_bit(Rule::AnnularTarget)));
            auto book = take(L::Spellbook::create());
            ok(book->add("burst", "Old disk"));
            old = take(book->edit("burst", read("S2.verse"), "", game(), learner))->accepted();
            auto h = take(R::ReferenceHost::create(game()));
            auto caster = take(h->add_actor(caster_spec()));
            centre = take(h->add_actor(target_spec({0, 0})));
            rune = take(h->install(plan(read("S5.verse")), caster));
            take(h->release(take(h->prepare(old->plan(), {caster, {}, R::Point{0, 0}}))));
            cp = take(h->checkpoint());
            take(learner.learn(Teaching{}.make()));
            auto changed = take(book->edit("burst", read("m5/burst.verse"), "", game(), learner));
            check(!changed->problem(), "new annular book revision accepts");
        }
        auto h = take(R::ReferenceHost::create(game()));
        ok(h->restore(cp));
        take(h->advance_to(100));
        check(
            h->snapshot().actors[centre.slot].data.health == 42 && h->snapshot().actors[centre.slot].slow == R::Slow{750, 300} &&
                h->items().items[rune.slot].charges == 1,
            "old disk centre included and armed item retained"
        );
        take(h->touch(take(h->admit_touch(1, rune, centre))));
        check(
            h->snapshot().actors[centre.slot].data.health == 54 && h->items().items[rune.slot].charges == 0 && old->revision() == 1 &&
                old->plan()->meaning().bindings[0].query == Query::EligibleDisk,
            "old inscription12 and old accepted disk unchanged"
        );
    });
    std::cout << passed << " annular composition cases passed; " << failures << " failed.\n";
    return failures ? 1 : 0;
}
