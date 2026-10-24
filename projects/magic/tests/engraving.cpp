#include <magic/reference-host.h>
#include "literals.h"

#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {
    using namespace Magic;
    using namespace Magic::Reference;
    int passed = 0, failed = 0;
    void check(bool value, std::string_view why) {
        if (!value)
            throw std::runtime_error(std::string(why));
    }
    template <class T> T take(std::expected<T, Failure> value) {
        if (!value)
            throw std::runtime_error(std::string(error_name(value.error().code)));
        return std::move(*value);
    }
    void ok(std::expected<void, Failure> value) {
        if (!value)
            throw std::runtime_error(std::string(error_name(value.error().code)));
    }
    template <class T> void rejects(const std::expected<T, Failure>& value, Error error) {
        check(!value, "expected refusal");
        if (value.error().code != error)
            throw std::runtime_error("expected " + std::string(error_name(error)) + ", got " + std::string(error_name(value.error().code)));
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
        c.operations = {{Primitive::Heal, "game.heal"}, {Primitive::Fire, "game.fire"}, {Primitive::Slow, "game.slow"}};
        return c;
    }
    PlanHandle compile_plan(std::string_view text, const Catalog& c = game()) {
        auto p = Magic::compile(text, c);
        if (!p)
            throw std::runtime_error(p.error().message);
        return *p;
    }
    PlanHandle fixture(std::string_view file) {
        std::ifstream in(std::string(MAGIC_FIXTURE_DIR) + "/" + std::string(file) + ".verse");
        check(in.good(), "fixture opens");
        return compile_plan(std::string(std::istreambuf_iterator<char>(in), {}));
    }
    const ActorSpec payer{{225, 0}, 100, 100, 100, 1000, false, true, true};
    const ActorSpec target{{250, 0}, 50, 100, 0, 1000, true, true, false};
    const ActorSpec other{{800, 0}, 20, 100, 0, 1000, true, true, false};
    struct Fixture {
        std::unique_ptr<ReferenceHost> h;
        ActorId p, t, o;
        PlanHandle spell = fixture("S5");
        explicit Fixture(HostLimits l = {}) {
            h = take(ReferenceHost::create(game(), l));
            p = take(h->add_actor(payer));
            t = take(h->add_actor(target));
            o = take(h->add_actor(other));
        }
        ItemId install() { return take(h->install(spell, p)); }
        template <class F> void unchanged(F body) {
            const auto w = h->snapshot();
            const auto i = h->items();
            body();
            check(w == h->snapshot() && i == h->items(), "refusal preserves complete world/items/ledger");
        }
    };
    Snapshot literal_world(std::uint64_t revision, std::uint32_t mana = 90, std::uint32_t hp = 50) {
        Snapshot s;
        s.revision = revision;
        for (std::size_t i = 0; i < actor_capacity; ++i)
            s.actors[i].id.slot = static_cast<std::uint32_t>(i);
        s.actors[0] = {{0, 1}, true, {{225, 0}, 100, 100, mana, 1000, false, true, true}, {1000, 0}};
        s.actors[1] = {{1, 1}, true, {{250, 0}, hp, 100, 0, 1000, true, true, false}, {1000, 0}};
        s.actors[2] = {{2, 1}, true, {{800, 0}, 20, 100, 0, 1000, true, true, false}, {1000, 0}};
        return s;
    }
    ItemSnapshot literal_items(std::size_t plan_bytes, bool spent = false, bool admitted = false) {
        ItemSnapshot s;
        for (std::size_t i = 0; i < item_capacity; ++i)
            s.items[i].id.slot = static_cast<std::uint32_t>(i);
        s.items[0] = {{0, 1}, true, true, spent ? 0u : 1u, 10, plan_bytes};
        if (admitted) {
            s.events_issued = 1;
            s.ledger[0] = {17, 1, {0, 1}, {1, 1}, spent};
        }
        return s;
    }
}
int main() {
    static_assert(!std::is_convertible_v<Snapshot, Checkpoint>);
    static_assert(!std::is_convertible_v<std::uint64_t, TouchEvent>);
    test("literal armed/released/spent worlds and immutable report", [] {
        Fixture f;
        const auto id = f.install();
        check(f.h->snapshot() == literal_world(4), "literal armed world");
        check(f.h->items() == literal_items(f.spell->metrics().retained_bytes), "literal armed item");
        auto cap = take(f.h->admit_touch(17, id, f.t));
        check(f.h->snapshot() == literal_world(5), "admission has no effect/debit");
        auto report = take(f.h->touch(cap));
        check(report->before == literal_world(5) && report->after == literal_world(6, 90, 62), "independent literal touch world");
        check(report->items_before == literal_items(f.spell->metrics().retained_bytes, false, true), "literal admitted receipt");
        check(report->items_after == literal_items(f.spell->metrics().retained_bytes, true, true), "literal spent receipt");
        const auto saved = *report;
        take(f.h->advance_to(1));
        f.h.reset();
        check(report->after == saved.after && report->items_after == saved.items_after, "old output owns all values");
    });
    test("accepted event binds actual toucher rather than installer/previous target", [] {
        Fixture f;
        auto id = f.install();
        auto event = take(f.h->admit_touch(1, id, f.o));
        take(f.h->touch(event));
        check(
            f.h->snapshot().actors[2].data.health == 32 && f.h->snapshot().actors[1].data.health == 50 &&
                f.h->snapshot().actors[0].data.health == 100,
            "only event actor heals"
        );
    });
    test("unwilling ineligible and dead admission stays usable after explicit correction", [] {
        for (int change = 0; change < 3; ++change) {
            Fixture f;
            auto id = f.install();
            auto e = take(f.h->admit_touch(1, id, f.t));
            auto data = target;
            if (change == 0)
                data.willing = false;
            if (change == 1)
                data.eligible = false;
            if (change == 2)
                data.health = 0;
            ok(f.h->update_actor(f.t, data));
            f.unchanged([&] { rejects(f.h->touch(e), Error::Contact); });
            ok(f.h->update_actor(f.t, target));
            take(f.h->touch(e));
            check(f.h->snapshot().actors[1].data.health == 62, "same admission succeeds once");
        }
    });
    test("installation funds exactly10 once; nine and permission refuse", [] {
        Fixture f;
        auto data = payer;
        data.mana = 9;
        ok(f.h->update_actor(f.p, data));
        f.unchanged([&] { rejects(f.h->install(f.spell, f.p), Error::Resources); });
        data.mana = 10;
        data.can_cast = false;
        ok(f.h->update_actor(f.p, data));
        f.unchanged([&] { rejects(f.h->install(f.spell, f.p), Error::Permission); });
        data.can_cast = true;
        ok(f.h->update_actor(f.p, data));
        auto id = f.install();
        check(f.h->snapshot().actors[0].data.mana == 0, "exact funding");
        take(f.h->touch(take(f.h->admit_touch(1, id, f.t))));
        check(f.h->snapshot().actors[0].data.mana == 0, "touch never bills a fake caster");
    });
    test("live key primitive quantity and capability changes refuse atomically", [] {
        for (int change = 0; change < 6; ++change) {
            Fixture f;
            auto id = f.install();
            auto e = take(f.h->admit_touch(1, id, f.t));
            auto c = game();
            if (change == 0)
                c.operations[0].key = "changed.heal";
            if (change == 1) {
                c.operations.erase(c.operations.begin() + 1);
                c.operations[0].primitive = Primitive::Fire;
            }
            if (change == 2)
                c.operations[0].max_quantity = 11;
            if (change == 3)
                c.engraved = false;
            if (change == 4)
                c.item_event = false;
            if (change == 5)
                c.operations.erase(c.operations.begin());
            ok(f.h->replace_catalog(c));
            f.unchanged([&] { rejects(f.h->touch(e), Error::SchemaMismatch); });
            ok(f.h->replace_catalog(game()));
            take(f.h->touch(e));
        }
    });
    test("unknown owned key never dispatches solely by enum", [] {
        Fixture f;
        auto c = game();
        c.operations[0].key = "foreign.heal";
        auto p = compile_plan(Literal::S5, c);
        f.unchanged([&] { rejects(f.h->install(p, f.p), Error::SchemaMismatch); });
    });
    test("default foreign copied and repeated capabilities", [] {
        Fixture a, b;
        auto i = a.install();
        auto e = take(a.h->admit_touch(17, i, a.t));
        auto copy = e;
        a.unchanged([&] { rejects(a.h->touch(TouchEvent{}), Error::InvalidEvent); });
        b.unchanged([&] { rejects(b.h->touch(e), Error::ForeignEvent); });
        take(a.h->touch(e));
        a.unchanged([&] {
            rejects(a.h->touch(copy), Error::RepeatedEvent);
            rejects(a.h->resume_touch(17), Error::RepeatedEvent);
            rejects(a.h->admit_touch(17, i, a.t), Error::DuplicateFact);
        });
    });
    test("disabled refusal preserves admission; spent stays visible and inert", [] {
        Fixture f;
        auto i = f.install();
        auto e = take(f.h->admit_touch(1, i, f.t));
        ok(f.h->enable_item(i, false));
        f.unchanged([&] { rejects(f.h->touch(e), Error::DisabledItem); });
        ok(f.h->enable_item(i, true));
        take(f.h->touch(e));
        auto another = take(f.h->admit_touch(2, i, f.t));
        f.unchanged([&] { rejects(f.h->touch(another), Error::SpentItem); });
        check(f.h->items().items[0].present && f.h->items().items[0].charges == 0, "spent retained");
    });
    test("removed/reused item generation rejects old fact and never refunds", [] {
        Fixture f;
        auto i = f.install();
        auto e = take(f.h->admit_touch(1, i, f.t));
        ok(f.h->remove_item(i));
        f.unchanged([&] { rejects(f.h->touch(e), Error::MissingItem); });
        auto replacement = f.install();
        check(
            replacement.slot == i.slot && replacement.generation == 2 && f.h->snapshot().actors[0].data.mana == 80, "reuse and no refund"
        );
        f.unchanged([&] {
            rejects(f.h->touch(e), Error::MissingItem);
            rejects(f.h->enable_item(i, true), Error::MissingItem);
        });
        take(f.h->touch(take(f.h->admit_touch(2, replacement, f.t))));
    });
    test("removed/reused toucher cannot retarget an old admission", [] {
        Fixture f;
        auto i = f.install();
        auto e = take(f.h->admit_touch(1, i, f.t));
        ok(f.h->remove_actor(f.t));
        auto replacement = take(f.h->add_actor(target));
        check(replacement.generation == 2, "reused actor");
        f.unchanged([&] { rejects(f.h->touch(e), Error::MissingActor); });
    });
    test("item generation horizon refuses first-free reuse without mutation", [] {
        HostLimits l;
        l.max_generation = 1;
        Fixture f(l);
        auto i = f.install();
        ok(f.h->remove_item(i));
        f.unchanged([&] { rejects(f.h->install(f.spell, f.p), Error::GenerationLimit); });
    });
    test("item exact capacity eight and one-short zero", [] {
        Fixture f;
        for (std::size_t n = 0; n < 8; ++n)
            check(f.install().slot == n, "first-free slots");
        f.unchanged([&] { rejects(f.h->install(f.spell, f.p), Error::ItemLimit); });
        HostLimits l;
        l.items = 0;
        Fixture z(l);
        z.unchanged([&] { rejects(z.h->install(z.spell, z.p), Error::ItemLimit); });
        l.items = 1;
        Fixture one(l);
        one.install();
        one.unchanged([&] { rejects(one.h->install(one.spell, one.p), Error::ItemLimit); });
    });
    test("receipt exact sixteen and one-short admission limits; no eviction", [] {
        Fixture f;
        auto i = f.install();
        auto first = take(f.h->admit_touch(1, i, f.t));
        take(f.h->touch(first));
        for (std::uint64_t n = 2; n <= 16; ++n)
            take(f.h->admit_touch(n, i, f.t));
        f.unchanged([&] {
            rejects(f.h->admit_touch(17, i, f.t), Error::EventLimit);
            rejects(f.h->touch(first), Error::RepeatedEvent);
            rejects(f.h->admit_touch(1, i, f.t), Error::DuplicateFact);
        });
        HostLimits l;
        l.touch_receipts = 0;
        Fixture z(l);
        auto zi = z.install();
        z.unchanged([&] { rejects(z.h->admit_touch(1, zi, z.t), Error::EventLimit); });
        l.touch_receipts = 15;
        Fixture s(l);
        auto si = s.install();
        for (std::uint64_t n = 1; n <= 15; ++n)
            take(s.h->admit_touch(n, si, s.t));
        s.unchanged([&] { rejects(s.h->admit_touch(16, si, s.t), Error::EventLimit); });
    });
    test("nonce and fact horizons exact and one-short without wrap", [] {
        HostLimits l;
        l.max_event_nonce = 1;
        l.max_fact_id = 7;
        Fixture f(l);
        auto i = f.install();
        take(f.h->admit_touch(7, i, f.t));
        f.unchanged([&] {
            rejects(f.h->admit_touch(0, i, f.t), Error::InvalidEvent);
            rejects(f.h->admit_touch(8, i, f.t), Error::InvalidEvent);
            rejects(f.h->admit_touch(6, i, f.t), Error::NonceLimit);
        });
        l.max_event_nonce = 0;
        Fixture z(l);
        auto zi = z.install();
        z.unchanged([&] { rejects(z.h->admit_touch(1, zi, z.t), Error::NonceLimit); });
    });
    test("installation refuses wrong envelope and missing plan", [] {
        Fixture f;
        f.unchanged([&] {
            rejects(f.h->install({}, f.p), Error::NoPlan);
            rejects(f.h->install(fixture("S1"), f.p), Error::UnsupportedEntry);
        });
    });
    test("whole payload shares slow/health logic; conflicting bindings cannot partially heal", [] {
        Fixture f;
        const std::string prefix =
            "engraved item-touch charges 1 release via item-event after 0 ms { bind a = willing event toucher at activation ; ";
        auto combined = compile_plan(
            prefix +
            "together ordered { on a bridge Common@1 { heal 12 hp instant } ; on a bridge Common@1 { multiply speed by 750 permille for 2000 ms } ; } }"
        );
        auto id = take(f.h->install(combined, f.p));
        auto e = take(f.h->admit_touch(1, id, f.t));
        take(f.h->touch(e));
        check(f.h->snapshot().actors[1].data.health == 62 && f.h->snapshot().actors[1].slow == Slow{750, 200}, "shared writes");
        auto second = take(f.h->install(combined, f.p));
        auto e2 = take(f.h->admit_touch(2, second, f.t));
        f.unchanged([&] { rejects(f.h->touch(e2), Error::ActiveSlow); });
        take(f.h->advance_to(200));
        take(f.h->touch(e2));
        auto conflict = compile_plan(
            prefix +
            "bind b = willing event toucher at activation ; together ordered { on a bridge Common@1 { heal 12 hp instant } ; on b bridge Common@1 { fire damage 8 dp instant } ; } }"
        );
        auto third = take(f.h->install(conflict, f.p));
        auto e3 = take(f.h->admit_touch(3, third, f.t));
        f.unchanged([&] { rejects(f.h->touch(e3), Error::Overlap); });
    });
    test("plan byte quota exact and one-short on installation", [] {
        auto p = fixture("S5");
        HostLimits l;
        l.plan_bytes = p->metrics().retained_bytes;
        Fixture f(l);
        take(f.h->install(p, f.p));
        --l.plan_bytes;
        Fixture s(l);
        s.unchanged([&] { rejects(s.h->install(p, s.p), Error::PlanLimit); });
    });
    test("touch work exact boundary and one-short preserve charge and receipt", [] {
        Fixture probe;
        auto i = probe.install();
        auto usage = take(probe.h->touch(take(probe.h->admit_touch(1, i, probe.t))))->usage;
        for (auto cap : {usage.work, usage.work - 1}) {
            HostLimits l;
            l.work = cap;
            Fixture f(l);
            auto id = f.install();
            auto e = take(f.h->admit_touch(1, id, f.t));
            if (cap == usage.work)
                take(f.h->touch(e));
            else
                f.unchanged([&] { rejects(f.h->touch(e), Error::WorkLimit); });
        }
        std::cout << "touch exact work=" << usage.work << '\n';
    });
    test("touch selector and selected target one-short", [] {
        HostLimits exact;
        exact.selector_work = 1;
        exact.selected_targets = 1;
        Fixture positive(exact);
        auto pi = positive.install();
        take(positive.h->touch(take(positive.h->admit_touch(1, pi, positive.t))));
        for (int change = 0; change < 2; ++change) {
            HostLimits l;
            if (change == 0)
                l.selector_work = 0;
            else
                l.selected_targets = 0;
            Fixture f(l);
            auto i = f.install();
            auto e = take(f.h->admit_touch(1, i, f.t));
            f.unchanged([&] { rejects(f.h->touch(e), change == 0 ? Error::SelectorLimit : Error::TargetLimit); });
        }
    });
    test("owned armed checkpoint survives source catalog and original owner destruction", [] {
        Checkpoint armed;
        ItemId id;
        ActorId toucher;
        std::weak_ptr<const Plan> retained;
        {
            auto c = game();
            std::string source(Literal::S5);
            auto p = compile_plan(source, c);
            retained = p;
            Fixture f;
            f.spell.reset();
            id = take(f.h->install(p, f.p));
            toucher = f.t;
            armed = take(f.h->checkpoint());
            source.assign("destroyed");
            c.operations.clear();
        }
        check(!retained.expired(), "checkpoint retains actual plan");
        auto h = take(ReferenceHost::create(game()));
        ok(h->restore(armed));
        check(h->snapshot() == literal_world(4), "literal restored armed world");
        auto report = take(h->touch(take(h->admit_touch(17, id, toucher))));
        check(report->after == literal_world(6, 90, 62), "actual restored touch");
        auto spent = take(h->checkpoint());
        h.reset();
        h = take(ReferenceHost::create(game()));
        ok(h->restore(spent));
        check(h->snapshot() == literal_world(6, 90, 62) && h->items() == report->items_after, "literal restored spent state");
        auto e = take(h->admit_touch(18, id, toucher));
        const auto before = h->snapshot();
        const auto items = h->items();
        rejects(h->touch(e), Error::SpentItem);
        check(h->snapshot() == before && h->items() == items, "spent refusal unchanged");
    });
    test("restore fresh authority rejects both old tickets and event copies; resume explicit", [] {
        Fixture f;
        auto id = f.install();
        auto e = take(f.h->admit_touch(17, id, f.t));
        auto ticket = take(f.h->prepare(fixture("S1"), {f.p, f.t, {}}));
        auto cp = take(f.h->checkpoint());
        ok(f.h->restore(cp));
        f.unchanged([&] {
            rejects(f.h->release(ticket), Error::ForeignTicket);
            rejects(f.h->touch(e), Error::ForeignEvent);
            rejects(f.h->admit_touch(17, id, f.t), Error::DuplicateFact);
        });
        auto resumed = take(f.h->resume_touch(17));
        take(f.h->touch(resumed));
        f.unchanged([&] { rejects(f.h->touch(resumed), Error::RepeatedEvent); });
    });
    test("explicit whole-world rewind restores charges but not old live authority", [] {
        Fixture f;
        auto id = f.install();
        auto cp = take(f.h->checkpoint());
        auto e = take(f.h->admit_touch(17, id, f.t));
        take(f.h->touch(e));
        ok(f.h->restore(cp));
        check(f.h->snapshot() == literal_world(4), "rewind is explicit world replacement");
        rejects(f.h->touch(e), Error::ForeignEvent);
        take(f.h->touch(take(f.h->admit_touch(17, id, f.t))));
        check(f.h->snapshot() == literal_world(6, 90, 62), "new timeline has one delivery");
    });
    test("checkpoint retains ledger tombstones across removal/replacement", [] {
        Fixture f;
        auto id = f.install();
        take(f.h->admit_touch(17, id, f.t));
        ok(f.h->remove_item(id));
        auto replacement = f.install();
        auto cp = take(f.h->checkpoint());
        ok(f.h->restore(cp));
        f.unchanged([&] {
            rejects(f.h->resume_touch(17), Error::MissingItem);
            rejects(f.h->admit_touch(17, replacement, f.t), Error::DuplicateFact);
        });
    });
    test("default/incompatible schema/limits restore preserves old world and casting ticket", [] {
        Fixture source;
        source.install();
        auto cp = take(source.h->checkpoint());
        Fixture f;
        auto ticket = take(f.h->prepare(fixture("S1"), {f.p, f.t, {}}));
        f.unchanged([&] { rejects(f.h->restore(Checkpoint{}), Error::InvalidCheckpoint); });
        auto c = game();
        c.operations[0].key = "changed.heal";
        ok(f.h->replace_catalog(c));
        f.unchanged([&] { rejects(f.h->restore(cp), Error::IncompatibleCheckpoint); });
        ok(f.h->replace_catalog(game()));
        // Catalog owner edits deliberately stale the original ticket; use a newly prepared one for limit mismatch.
        ticket = take(f.h->prepare(fixture("S1"), {f.p, f.t, {}}));
        HostLimits l;
        l.items = 7;
        Fixture different(l);
        auto incompatible = take(different.h->checkpoint());
        f.unchanged([&] { rejects(f.h->restore(incompatible), Error::IncompatibleCheckpoint); });
        take(f.h->release(ticket));
        ok(f.h->restore(cp));
        check(f.h->snapshot() == literal_world(4), "prior checkpoint still usable");
    });
    test("checkpoint rejects currently unsupported retained schema and departed pending caster", [] {
        Fixture f;
        f.install();
        auto c = game();
        c.item_event = false;
        ok(f.h->replace_catalog(c));
        f.unchanged([&] { rejects(f.h->checkpoint(), Error::SchemaMismatch); });
        ok(f.h->replace_catalog(game()));
        take(f.h->checkpoint());
        Fixture queued;
        take(queued.h->release(take(queued.h->prepare(fixture("S2"), {queued.p, {}, Point{0, 0}}))));
        ok(queued.h->remove_actor(queued.p));
        queued.unchanged([&] { rejects(queued.h->checkpoint(), Error::MissingActor); });
        take(queued.h->cancel(1));
        take(queued.h->checkpoint());
    });
    test("pending plan captured aim due tick active slow and expiry survive checkpoints", [] {
        Checkpoint pending;
        std::weak_ptr<const Plan> retained;
        {
            Fixture f;
            auto p = fixture("S2");
            retained = p;
            take(f.h->release(take(f.h->prepare(p, {f.p, {}, Point{0, 0}}))));
            take(f.h->advance_to(99));
            pending = take(f.h->checkpoint());
        }
        check(!retained.expired(), "checkpoint retains scheduled plan");
        auto h = take(ReferenceHost::create(game()));
        ok(h->restore(pending));
        auto expected = literal_world(4, 80);
        expected.tick = 99;
        expected.revision = 5;
        expected.actions_issued = 1;
        expected.pending[0] = {true, 1, 100, {0, 1}, {0, 0}, retained.lock()->metrics().retained_bytes};
        check(h->snapshot() == expected, "literal restored pending world");
        pending = Checkpoint{};
        auto activated = take(h->advance_to(100));
        expected.tick = 100;
        expected.revision = 6;
        expected.pending[0] = {};
        expected.actors[1].data.health = 42;
        expected.actors[1].slow = {750, 300};
        check(activated->after == expected && activated->event_count == 1, "literal exact onset");
        check(retained.expired(), "no plan regeneration or leaked queue handle");
        auto active = take(h->checkpoint());
        h.reset();
        h = take(ReferenceHost::create(game()));
        ok(h->restore(active));
        take(h->advance_to(299));
        expected.tick = 299;
        expected.revision = 7;
        check(h->snapshot() == expected, "literal before expiry");
        take(h->advance_to(300));
        expected.tick = 300;
        expected.revision = 8;
        expected.actors[1].slow = {};
        check(h->snapshot() == expected, "literal expiry");
        check(take(h->advance_to(300))->event_count == 0 && h->snapshot() == expected, "no replay");
    });
    test("checkpoint state work bound refuses without changing caller outputs", [] {
        HostLimits l;
        l.work = 300; // Valid actor setup under new fixed copy244; checkpoint still exceeds the allowance.
        Fixture f(l);
        const auto before = f.h->snapshot();
        rejects(f.h->checkpoint(), Error::WorkLimit);
        check(f.h->snapshot() == before, "capture refusal preserves world");
        Fixture source;
        auto cp = take(source.h->checkpoint());
        f.unchanged([&] { rejects(f.h->restore(cp), Error::WorkLimit); });
    });
    test("one checkpoint coherently retains items receipts slow and pending spoken work", [] {
        Checkpoint cp;
        ItemId heal;
        ActorId target_id, other_id;
        {
            Fixture f;
            target_id = f.t;
            other_id = f.o;
            auto slow = compile_plan(R"(engraved item-touch charges 1 release via item-event after 0 ms {
                bind a = willing event toucher at activation ; together ordered {
                    on a bridge Common@1 { multiply speed by 750 permille for 2000 ms } ; } })");
            auto slow_item = take(f.h->install(slow, f.p));
            take(f.h->touch(take(f.h->admit_touch(1, slow_item, f.o))));
            heal = f.install();
            take(f.h->release(take(f.h->prepare(fixture("S2"), {f.p, {}, Point{0, 0}}))));
            take(f.h->advance_to(99));
            cp = take(f.h->checkpoint());
        }
        auto h = take(ReferenceHost::create(game()));
        ok(h->restore(cp));
        check(
            h->snapshot().tick == 99 && h->snapshot().actors[0].data.mana == 60 &&
                h->snapshot().actors[other_id.slot].slow == Slow{750, 200},
            "restored clock/funds/active status"
        );
        check(
            h->items().items[0].charges == 0 && h->items().items[1].charges == 1 && h->items().ledger[0].consumed,
            "restored charges/receipt"
        );
        auto before = h->snapshot();
        auto items = h->items();
        rejects(h->admit_touch(1, heal, target_id), Error::DuplicateFact);
        check(h->snapshot() == before && h->items() == items, "receipt replay has no writes");
        take(h->touch(take(h->admit_touch(2, heal, target_id))));
        take(h->advance_to(100));
        check(
            h->snapshot().actors[target_id.slot].data.health == 54 && h->snapshot().actors[target_id.slot].slow == Slow{750, 300},
            "heal and scheduled fire/slow share real state"
        );
        take(h->advance_to(200));
        check(
            h->snapshot().actors[other_id.slot].slow == Slow{} && h->snapshot().actors[target_id.slot].slow == Slow{750, 300},
            "distinct expiry continuation"
        );
    });
    test("checkpoint refuses intrinsic pending duration beyond time horizon", [] {
        HostLimits l;
        l.max_tick = 299;
        Fixture f(l);
        take(f.h->release(take(f.h->prepare(fixture("S2"), {f.p, {}, Point{0, 0}}))));
        f.unchanged([&] { rejects(f.h->checkpoint(), Error::TimeLimit); });
        take(f.h->cancel(1));
        take(f.h->checkpoint());
    });
    test("checkpoint capture and restore work exact348 and one-short347", [] {
        HostLimits l;
        l.work = 632; // Historical name retained; M7 fixed copy280 plus full empty continuation scans.
        Fixture exact(l);
        auto cp = take(exact.h->checkpoint());
        ok(exact.h->restore(cp));
        l.work = 631;
        Fixture short_host(l);
        short_host.unchanged([&] { rejects(short_host.h->checkpoint(), Error::WorkLimit); });
    });
    test("revision exhaustion preserves touch charge and admission", [] {
        HostLimits l;
        l.max_revision = 5;
        Fixture f(l);
        auto id = f.install();
        auto e = take(f.h->admit_touch(1, id, f.t));
        f.unchanged([&] { rejects(f.h->touch(e), Error::RevisionLimit); });
    });
    test("nonzero accepted aim remains fixed across owner movement and destruction", [] {
        Checkpoint cp;
        {
            Fixture f;
            auto moved = target;
            moved.position = {1250, -500};
            ok(f.h->update_actor(f.t, moved));
            take(f.h->release(take(f.h->prepare(fixture("S2"), {f.p, {}, Point{1000, -500}}))));
            auto caster = payer;
            caster.position = {-900000, 900000};
            caster.mana = 80;
            ok(f.h->update_actor(f.p, caster));
            cp = take(f.h->checkpoint());
        }
        auto h = take(ReferenceHost::create(game()));
        ok(h->restore(cp));
        check(
            h->snapshot().pending[0].aim == Point{1000, -500} && h->snapshot().pending[0].due_tick == 100, "nonzero captured point and due"
        );
        take(h->advance_to(99));
        check(h->snapshot().actors[1].data.health == 50, "before onset");
        take(h->advance_to(100));
        check(
            h->snapshot().actors[1].data.health == 42 && h->snapshot().actors[1].slow == Slow{750, 300},
            "inclusive boundary at captured point"
        );
    });
    test("restoration retains issuance and generation exhaustion horizons", [] {
        HostLimits l;
        l.max_event_nonce = 1;
        l.max_generation = 1;
        Fixture f(l);
        auto id = f.install();
        auto e = take(f.h->admit_touch(17, id, f.t));
        take(f.h->touch(e));
        auto cp = take(f.h->checkpoint());
        ok(f.h->restore(cp));
        f.unchanged([&] {
            rejects(f.h->admit_touch(18, id, f.t), Error::NonceLimit);
            rejects(f.h->resume_touch(17), Error::RepeatedEvent);
        });
        ok(f.h->remove_item(id));
        f.unchanged([&] { rejects(f.h->install(f.spell, f.p), Error::GenerationLimit); });
    });
    test("new limit ceilings refuse rather than raising storage", [] {
        HostLimits l;
        l.items = 9;
        rejects(ReferenceHost::create(game(), l), Error::InvalidLimits);
        l = HostLimits{};
        l.touch_receipts = 17;
        rejects(ReferenceHost::create(game(), l), Error::InvalidLimits);
        l = HostLimits{};
        ++l.max_event_nonce;
        rejects(ReferenceHost::create(game(), l), Error::InvalidLimits);
    });
    std::cout << passed << " engraving/checkpoint cases passed; " << failed << " failed.\n";
    return failed ? 1 : 0;
}
