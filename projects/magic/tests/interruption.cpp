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
    template <class T, class E, class C> void refuses(std::expected<T, E> v, C code) {
        check(!v);
        check(v.error().code == code);
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
        c.operations = {{Primitive::Fire, "fire"}, {Primitive::Interrupt, "interrupt"}, {Primitive::Heal, "heal"}};
        return c;
    }
    const std::string fire =
        "spoken release via burst after 1000 ms { bind near = eligible actors in G0 disk 250 mm around aim captured at release queried at activation inclusive ; together ordered { on near bridge Common@1 { fire damage 4 dp instant } ; } }";
    const std::string interrupt =
        "spoken release via pending after 0 ms { bind cast = pending action captured at release ; together ordered { on cast bridge Common@1 { interrupt bound pending action instant } ; } }";
    std::string replace(std::string s, std::string_view a, std::string_view b) {
        auto i = s.find(a);
        check(i != s.npos);
        s.replace(i, a.size(), b);
        return s;
    }
    struct Fixture {
        std::unique_ptr<R::ReferenceHost> host;
        R::ActorId caster, target;
        PlanHandle damage, stop;
        Fixture(R::HostLimits l = {}, std::uint32_t mana = 100) {
            host = take(R::ReferenceHost::create(game(), l));
            caster = take(host->add_actor({{0, 0}, 100, 100, mana, 1000, false, true, true}));
            target = take(host->add_actor({{150, 0}, 50}));
            damage = take(compile(fire, game()));
            stop = take(compile(interrupt, game()));
        }
        R::Observed cast(PlanHandle p, R::Request r) { return take(host->release(take(host->prepare(p, r)))); }
        R::Observed queue() { return cast(damage, {caster, {}, R::Point{0, 0}}); }
        R::Request request(R::PendingRef ref) const { return {caster, {}, {}, {}, {}, {}, ref}; }
    };
}
int main() {
    test("literal typed pending role and exact Common requirement roundtrip", [] {
        auto p = take(compile(interrupt, game()));
        const auto& m = p->meaning();
        check(
            m.delivery == Delivery::Pending && m.onset_ticks == 0 && m.bindings[0].query == Query::CapturedPending &&
            m.bindings[0].query_time == BindingTime::Release &&
            m.effects[0] == Effect{Primitive::Interrupt, "interrupt", 0, 0, 0, Interruption{}}
        );
        check(
            p->requirements() == (rule_bit(Rule::CommonInterrupt) | rule_bit(Rule::TargetBridge)) &&
            take(compile(p->common(), game()))->meaning() == m
        );
    });
    test("authorized interrupt removes exactly paid pending action with owned result", [] {
        Fixture f;
        auto queued = f.queue();
        auto ref = take(f.host->bind_pending(1));
        auto preview = take(f.host->preview(f.stop, f.request(ref)));
        check(preview.mana_cost == 10 && preview.due_tick == 0 && f.host->snapshot().pending[0].present);
        auto stopped = f.cast(f.stop, f.request(ref));
        check(
            stopped->interrupted_action == 1 && stopped->events[0].action == 2 && f.host->snapshot().actors[0].data.mana == 80 &&
            !f.host->snapshot().pending[0].present && queued->after.pending[0].present
        );
        take(f.host->advance_to(100));
        check(f.host->snapshot().actors[1].data.health == 50 && stopped->after.tick == 0);
    });
    test("default foreign missing and repeated refs grant no second debit", [] {
        Fixture f, g;
        f.queue();
        auto ref = take(f.host->bind_pending(1));
        auto before = f.host->snapshot();
        refuses(f.host->prepare(f.stop, f.request({})), R::Error::InvalidCapability);
        refuses(g.host->prepare(g.stop, g.request(ref)), R::Error::ForeignCapability);
        refuses(f.host->bind_pending(0), R::Error::MissingAction);
        refuses(f.host->bind_pending(2), R::Error::MissingAction);
        check(f.host->snapshot() == before);
        f.cast(f.stop, f.request(ref));
        before = f.host->snapshot();
        refuses(f.host->prepare(f.stop, f.request(ref)), R::Error::MissingAction);
        check(f.host->snapshot() == before);
    });
    test("completed and administrative cancelled refs cannot retarget replacement slot", [] {
        Fixture f;
        f.queue();
        auto completed = take(f.host->bind_pending(1));
        take(f.host->advance_to(100));
        check(f.host->snapshot().actors[1].data.health == 46);
        refuses(f.host->prepare(f.stop, f.request(completed)), R::Error::MissingAction);
        f.queue();
        auto cancelled = take(f.host->bind_pending(2));
        take(f.host->cancel(2));
        f.queue();
        check(f.host->snapshot().pending[0].action == 3);
        auto before = f.host->snapshot();
        refuses(f.host->prepare(f.stop, f.request(cancelled)), R::Error::MissingAction);
        check(f.host->snapshot() == before);
        f.cast(f.stop, f.request(take(f.host->bind_pending(3))));
        check(f.host->snapshot().actors[1].data.health == 46);
    });
    test("schema key primitive subject and live permission refuse before payment", [] {
        for (unsigned mode = 0; mode < 4; ++mode) {
            Fixture f;
            f.queue();
            auto ref = take(f.host->bind_pending(1));
            auto c = game();
            if (mode == 0)
                c.operations[1].key = "other";
            if (mode == 1) {
                c.operations[1].primitive = Primitive::Heal;
                c.operations.pop_back();
            }
            if (mode == 2)
                c.subject = false;
            if (mode < 3)
                ok(f.host->replace_catalog(c));
            else {
                auto a = f.host->snapshot().actors[0].data;
                a.can_cast = false;
                ok(f.host->update_actor(f.caster, a));
            }
            auto before = f.host->snapshot();
            refuses(f.host->prepare(f.stop, f.request(ref)), mode == 3 ? R::Error::Permission : R::Error::SchemaMismatch);
            check(f.host->snapshot() == before);
        }
    });
    test("exact remaining mana10 and one-short9 preserve prior paid queue", [] {
        Fixture exact({}, 20);
        exact.queue();
        exact.cast(exact.stop, exact.request(take(exact.host->bind_pending(1))));
        check(exact.host->snapshot().actors[0].data.mana == 0);
        Fixture less({}, 19);
        less.queue();
        auto ref = take(less.host->bind_pending(1));
        auto before = less.host->snapshot();
        refuses(less.host->prepare(less.stop, less.request(ref)), R::Error::Resources);
        check(less.host->snapshot() == before && before.actors[0].data.mana == 9);
        take(less.host->advance_to(100));
        check(less.host->snapshot().actors[1].data.health == 46);
    });
    test("release exact340 work one-short339 preserves usable ticket and queue", [] {
        for (auto work : {340u, 339u}) {
            R::HostLimits l;
            l.work = work;
            Fixture f(l);
            f.queue();
            auto ref = take(f.host->bind_pending(1));
            auto ticket = take(f.host->prepare(f.stop, f.request(ref)));
            auto before = f.host->snapshot();
            if (work == 340) {
                auto result = take(f.host->release(ticket));
                check(result->usage.work == 340 && result->interrupted_action == 1);
            } else {
                refuses(f.host->release(ticket), R::Error::WorkLimit);
                check(f.host->snapshot() == before);
                ok(f.host->discard(ticket));
                take(f.host->cancel(1));
                check(f.host->snapshot().actors[0].data.mana == 90);
            }
        }
    });
    test("exact selected target1 and zero refuse without lost queue", [] {
        R::HostLimits l;
        l.selected_targets = 0;
        Fixture f(l);
        f.queue();
        auto ref = take(f.host->bind_pending(1));
        auto before = f.host->snapshot();
        refuses(f.host->prepare(f.stop, f.request(ref)), R::Error::TargetLimit);
        check(f.host->snapshot() == before);
    });
    test("eight admitted queue entries remove one exact slot preserve other seven", [] {
        Fixture f;
        for (unsigned i = 0; i < 8; ++i)
            f.queue();
        auto ref = take(f.host->bind_pending(8));
        auto result = f.cast(f.stop, f.request(ref));
        check(result->interrupted_action == 8 && f.host->snapshot().actors[0].data.mana == 10);
        for (unsigned i = 0; i < 7; ++i)
            check(f.host->snapshot().pending[i].present && f.host->snapshot().pending[i].action == i + 1);
        check(!f.host->snapshot().pending[7].present);
        take(f.host->advance_to(100));
        check(f.host->snapshot().actors[1].data.health == 22);
    });
    test("restore revokes refs tickets retains owned plan and intentional timeline rewind", [] {
        R::Checkpoint cp;
        R::PendingRef ref;
        R::Ticket ticket;
        R::ActorId caster, target;
        R::Observed queued;
        PlanHandle stop;
        {
            Fixture f;
            queued = f.queue();
            ref = take(f.host->bind_pending(1));
            ticket = take(f.host->prepare(f.stop, f.request(ref)));
            cp = take(f.host->checkpoint());
            caster = f.caster;
            target = f.target;
            stop = f.stop;
        }
        auto host = take(R::ReferenceHost::create(game()));
        ok(host->restore(cp));
        refuses(host->release(ticket), R::Error::ForeignTicket);
        refuses(host->prepare(stop, {caster, {}, {}, {}, {}, {}, ref}), R::Error::ForeignCapability);
        auto fresh = take(host->bind_pending(1));
        take(host->release(take(host->prepare(stop, {caster, {}, {}, {}, {}, {}, fresh}))));
        take(host->advance_to(100));
        check(
            host->snapshot().actors[target.slot].data.health == 50 && host->snapshot().actors[0].data.mana == 80 &&
            queued->after.pending[0].present
        );
        ok(host->restore(cp));
        refuses(host->prepare(stop, {caster, {}, {}, {}, {}, {}, fresh}), R::Error::ForeignCapability);
        take(host->advance_to(100));
        check(host->snapshot().actors[target.slot].data.health == 46 && host->snapshot().actors[0].data.mana == 90);
    });
    test("interruption can explicitly unblock a failed paid activation", [] {
        Fixture f;
        f.queue();
        auto a = f.host->snapshot().actors[1].data;
        a.eligible = false;
        ok(f.host->update_actor(f.target, a));
        auto before = f.host->snapshot();
        refuses(f.host->advance_to(100), R::Error::EmptySelection);
        check(f.host->snapshot() == before && before.tick == 0 && before.actors[0].data.mana == 90);
        auto ref = take(f.host->bind_pending(1));
        f.cast(f.stop, f.request(ref));
        take(f.host->advance_to(100));
        check(f.host->snapshot().actors[1].data.health == 50 && f.host->snapshot().actors[0].data.mana == 80);
    });
    test("generation-bound former caster cannot alias a newer action", [] {
        Fixture f;
        f.queue();
        auto old = take(f.host->bind_pending(1));
        ok(f.host->remove_actor(f.caster));
        f.caster = take(f.host->add_actor({{0, 0}, 100, 100, 100, 1000, false, true, true}));
        check(f.caster == R::ActorId{0, 2});
        f.cast(f.stop, f.request(old));
        f.queue();
        auto before = f.host->snapshot();
        refuses(f.host->prepare(f.stop, f.request(old)), R::Error::MissingAction);
        check(f.host->snapshot() == before && f.host->snapshot().pending[0].action == 3);
        f.cast(f.stop, f.request(take(f.host->bind_pending(3))));
    });
    test("new pending form refuses delay engraving mixed or extraneous roles", [] {
        check(!compile(replace(interrupt, "after 0 ms", "after 10 ms"), game()));
        check(!compile(replace(interrupt, "spoken release", "engraved item-touch charges 1 release"), game()));
        check(!compile(replace(interrupt, "pending action captured", "influence captured"), game()));
        check(!compile(replace(interrupt, "} ; } }", "} ; on cast bridge Common@1 { heal 12 hp instant } ; } }"), game()));
        Fixture f;
        f.queue();
        auto ref = take(f.host->bind_pending(1));
        auto req = f.request(ref);
        req.aim = R::Point{0, 0};
        refuses(f.host->prepare(f.stop, req), R::Error::UnsupportedBinding);
        req = {f.caster, {}, R::Point{0, 0}, {}, {}, {}, ref};
        refuses(f.host->prepare(f.damage, req), R::Error::UnsupportedBinding);
    });
    test("book recognition knowledge and accepted plan still need live authority", [] {
        auto book = take(L::Spellbook::create());
        ok(book->add("stop", "A precise interruption"));
        auto unknown = take(L::Learner::create(all_rules & ~rule_bit(Rule::CommonInterrupt)));
        auto draft = take(book->edit("stop", interrupt, "Exact pending only.", game(), unknown));
        check(draft->problem() && draft->problem()->code == L::Error::UnknownKnowledge);
        auto known = take(L::Learner::create(all_rules));
        auto accepted = take(book->edit("stop", interrupt, "No refund.", game(), known))->accepted();
        auto bad = take(book->edit("stop", "interrupt all magic", "Repair scope.", game(), known));
        check(bad->problem() && bad->accepted() == accepted);
        Fixture f;
        f.queue();
        refuses(f.host->release({}), R::Error::InvalidTicket);
        f.cast(accepted->plan(), f.request(take(f.host->bind_pending(1))));
    });

    test("fourteen registered typed operations exact catalog ceiling", [] {
        Catalog catalog;
        for (unsigned i = 0; i < 14; ++i)
            catalog.operations.push_back({static_cast<Primitive>(i), "operation." + std::to_string(i)});
        check(bool(R::ReferenceHost::create(catalog)) && bool(compile(interrupt, catalog)));
        catalog.operations.push_back({Primitive::Interrupt, "too.many"});
        refuses(R::ReferenceHost::create(catalog), R::Error::InvalidCatalog);
        check(!compile(interrupt, catalog));
    });
    test("prepared interruption live schema and pending completion preserve payment", [] {
        Fixture f;
        f.queue();
        auto ref = take(f.host->bind_pending(1));
        auto ticket = take(f.host->prepare(f.stop, f.request(ref)));
        auto c = game();
        c.operations[1].key = "disabled";
        ok(f.host->replace_catalog(c));
        auto before = f.host->snapshot();
        refuses(f.host->release(ticket), R::Error::SchemaMismatch);
        check(f.host->snapshot() == before);
        ok(f.host->discard(ticket));
        ok(f.host->replace_catalog(game()));
        ticket = take(f.host->prepare(f.stop, f.request(ref)));
        take(f.host->advance_to(100));
        before = f.host->snapshot();
        refuses(f.host->release(ticket), R::Error::MissingAction);
        check(f.host->snapshot() == before && before.actors[1].data.health == 46 && before.actors[0].data.mana == 90);
        ok(f.host->discard(ticket));
    });
    std::cout << passed << " interruption cases passed; " << failed << " failed\n";
    return failed ? 1 : 0;
}
