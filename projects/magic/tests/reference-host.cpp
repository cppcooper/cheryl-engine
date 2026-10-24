#include <magic/reference-host.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace {
    using namespace Magic;
    using namespace Magic::Reference;
    int passed = 0, failed = 0;
    void check(bool condition, std::string_view message) {
        if (!condition)
            throw std::runtime_error(std::string(message));
    }
    void test(std::string_view name, const std::function<void()>& body) {
        try {
            body();
            ++passed;
            std::cout << "PASS " << name << '\n';
        } catch (const std::exception& error) {
            ++failed;
            std::cout << "FAIL " << name << ": " << error.what() << '\n';
        }
    }
    template <class T> T take(std::expected<T, Failure> result) {
        if (!result)
            throw std::runtime_error(std::string(error_name(result.error().code)));
        return std::move(*result);
    }
    void ok(std::expected<void, Failure> result) {
        if (!result)
            throw std::runtime_error(std::string(error_name(result.error().code)));
    }
    template <class T> Failure rejects(const std::expected<T, Failure>& result, Error code) {
        check(!result, "expected refusal");
        if (result.error().code != code)
            throw std::runtime_error("expected " + std::string(error_name(code)) + ", got " + std::string(error_name(result.error().code)));
        return result.error();
    }
    Catalog game() {
        Catalog catalog;
        catalog.operations = {{Primitive::Heal, "game.heal"}, {Primitive::Fire, "game.fire"}, {Primitive::Slow, "game.slow"}};
        return catalog;
    }
    PlanHandle plan(std::string_view text, const Catalog& catalog = game()) {
        auto compiled = Magic::compile(text, catalog);
        if (!compiled)
            throw std::runtime_error(compiled.error().message);
        return *compiled;
    }
    PlanHandle fixture(std::string_view name) {
        std::ifstream stream(std::string(MAGIC_FIXTURE_DIR) + "/" + std::string(name) + ".verse");
        check(stream.good(), "fixture opens");
        return plan(std::string(std::istreambuf_iterator<char>(stream), {}));
    }
    const ActorSpec caster_data{{225, 0}, 100, 100, 100, 1000, false, true, true};
    const ActorSpec target_data{{250, 0}, 50, 100, 0, 1000, true, true, false};
    const ActorSpec outer_data{{251, 0}, 80, 100, 0, 1000, true, true, false};
    struct Fixture {
        std::unique_ptr<ReferenceHost> host;
        ActorId caster, target, outer;
        explicit Fixture(HostLimits limits = {}, bool reverse_storage = false) {
            host = take(ReferenceHost::create(game(), limits));
            caster = take(host->add_actor(caster_data));
            if (reverse_storage) {
                outer = take(host->add_actor(outer_data));
                target = take(host->add_actor(target_data));
            } else {
                target = take(host->add_actor(target_data));
                outer = take(host->add_actor(outer_data));
            }
        }
        Request contact() const { return {caster, target, std::nullopt}; }
        Request burst(Point aim = {}) const { return {caster, std::nullopt, aim}; }
        Observed cast(const PlanHandle& value, const Request& request) { return take(host->release(take(host->prepare(value, request)))); }
    };
    Snapshot literal_before() {
        Snapshot result;
        result.revision = 3;
        for (std::size_t i = 0; i < actor_capacity; ++i)
            result.actors[i].id.slot = static_cast<std::uint32_t>(i);
        result.actors[0] = {{0, 1}, true, {{225, 0}, 100, 100, 100, 1000, false, true, true}, {1000, 0}};
        result.actors[1] = {{1, 1}, true, {{250, 0}, 50, 100, 0, 1000, true, true, false}, {1000, 0}};
        result.actors[2] = {{2, 1}, true, {{251, 0}, 80, 100, 0, 1000, true, true, false}, {1000, 0}};
        return result;
    }
    std::string disk_source(std::size_t bindings, std::string_view payload, std::uint32_t delay_ms = 1000) {
        std::string source = "spoken release via burst after " + std::to_string(delay_ms) + " ms { ";
        for (std::size_t i = 0; i < bindings; ++i)
            source += "bind b" + std::to_string(i) +
                      " = eligible actors in G0 disk 250 mm around aim captured at release queried at activation inclusive ; ";
        source += "together ordered { on b0 bridge Common@1 { " + std::string(payload) + " } ; } }";
        return source;
    }
}

int main() {
    static_assert(!std::is_copy_constructible_v<ReferenceHost> && !std::is_move_constructible_v<ReferenceHost>);
    static_assert(std::is_same_v<Observed::element_type, const Observation>);
    test("S1 full literal release and pure preview", [] {
        Fixture f;
        const auto value = fixture("S1");
        auto expected = literal_before();
        check(f.host->snapshot() == expected, "independent complete before state");
        auto preview = take(f.host->preview(value, f.contact()));
        check(
            preview.mana_cost == 10 && preview.due_tick == 0 && f.host->snapshot() == expected, "preview has no effects/resources/action"
        );
        auto ticket = take(f.host->prepare(value, f.contact()));
        check(f.host->snapshot() == expected, "prepare has no game effects/debit");
        const auto observation = take(f.host->release(ticket));
        check(observation->before == expected, "owned literal before");
        expected.revision = 4;
        expected.actions_issued = 1;
        expected.actors[0].data.mana = 90;
        expected.actors[1].data.health = 62;
        check(observation->after == expected && f.host->snapshot() == expected, "independent complete healed state");
        check(observation->events[0].action == 1 && observation->events[0].kind == EventKind::Applied, "accepted action identity");
    });
    for (const std::string name : {"S2", "S4"})
        test(name + " full literal release/onset/expiry", [name] {
            Fixture f;
            const auto value = fixture(name);
            auto expected = literal_before();
            const auto before = expected;
            auto release = f.cast(value, f.burst());
            expected.revision = 4;
            expected.actions_issued = 1;
            expected.actors[0].data.mana = 80;
            expected.pending[0] = {true,   1,
                                   100,    {0, 1},
                                   {0, 0}, value->metrics().retained_bytes}; // retained compiler metadata, not host-derived mechanics
            check(
                release->before == before && release->after == expected && f.host->snapshot() == expected,
                "literal queued state, no early effects"
            );
            const auto queued_literal = expected;
            auto at99 = take(f.host->advance_to(99));
            expected.tick = 99;
            expected.revision = 5;
            check(at99->after == expected && f.host->snapshot() == expected, "literal before onset tick 99");
            auto onset = take(f.host->advance_to(100));
            expected.tick = 100;
            expected.revision = 6;
            expected.pending[0] = {};
            expected.actors[1].data.health = 42;
            expected.actors[1].slow = {750, 300};
            check(onset->after == expected && f.host->snapshot() == expected, "literal onset fire8 + slow750, actor251 excluded");
            check(
                onset->events[0].kind == EventKind::Activated && onset->events[0].action == 1 && onset->events[0].tick == 100,
                "onset identity"
            );
            check(onset->after.actors[1].speed_milli_mm_per_s() == 750000, "exact 750 mm/s speed");
            auto last = take(f.host->advance_to(299));
            expected.tick = 299;
            expected.revision = 7;
            check(last->after == expected, "literal last active tick");
            auto expired = take(f.host->advance_to(300));
            expected.tick = 300;
            expected.revision = 8;
            expected.actors[1].slow = {1000, 0};
            check(expired->after == expected && f.host->snapshot() == expected, "literal exact 200-tick expiry");
            f.host.reset();
            check(
                release->after == queued_literal && onset->after.actors[1].slow == Slow{750, 300},
                "old observations immutable after host destruction"
            );
        });
    test("S3 immediate literal slow and expiry", [] {
        Fixture f;
        auto expected = literal_before();
        auto observed = f.cast(fixture("S3"), f.burst());
        expected.revision = 4;
        expected.actions_issued = 1;
        expected.actors[0].data.mana = 90;
        expected.actors[1].slow = {750, 200};
        check(observed->after == expected, "literal immediate speed only");
        expected.tick = 200;
        expected.revision = 5;
        expected.actors[1].slow = {1000, 0};
        check(take(f.host->advance_to(200))->after == expected, "literal immediate status expiry");
    });
    test("novel heal plus slow literal state", [] {
        Fixture f;
        auto expected = literal_before();
        f.cast(fixture("mixed-schools"), f.burst());
        expected.revision = 5;
        expected.actions_issued = 1;
        expected.tick = 100;
        expected.actors[0].data.mana = 80;
        expected.actors[1].data.health = 62;
        expected.actors[1].slow = {750, 300};
        check(take(f.host->advance_to(100))->after == expected, "novel composition really heals and slows");
    });
    test("current membership moved point boundary and eligibility", [] {
        Fixture f;
        f.cast(fixture("S2"), f.burst());
        auto departed = target_data;
        departed.position = {1000, 1000};
        ok(f.host->update_actor(f.target, departed));
        auto entered = outer_data;
        entered.position = {150, 200};
        ok(f.host->update_actor(f.outer, entered));
        auto caster = caster_data;
        caster.position = {-1000, -1000};
        ok(f.host->update_actor(f.caster, caster));
        auto observed = take(f.host->advance_to(100));
        check(observed->after.actors[1].data.health == 50 && observed->after.actors[1].slow == Slow{}, "departed target unaffected");
        check(
            observed->after.actors[2].data.health == 72 && observed->after.actors[2].slow == Slow{750, 300},
            "current actor at 150,200 inclusive radius250"
        );
        Fixture ineligible;
        ineligible.cast(fixture("S2"), ineligible.burst());
        auto target = target_data;
        target.eligible = false;
        ok(ineligible.host->update_actor(ineligible.target, target));
        auto before = ineligible.host->snapshot();
        auto error = rejects(ineligible.host->advance_to(100), Error::EmptySelection);
        check(
            error.action == 1 && error.tick == 100 && ineligible.host->snapshot() == before,
            "current eligibility refusal preserves complete step"
        );
    });
    test("removed target and generation replacement", [] {
        Fixture f;
        f.cast(fixture("S2"), f.burst());
        ok(f.host->remove_actor(f.target));
        auto replacement = take(f.host->add_actor(target_data));
        check(replacement.slot == f.target.slot && replacement.generation == 2, "identity generation advances");
        auto step = take(f.host->advance_to(100));
        check(
            step->after.actors[replacement.slot].data.health == 42 && step->after.actors[replacement.slot].slow.until_tick == 300,
            "current replacement selected by area, no stale pointers"
        );
        rejects(f.host->update_actor(f.target, target_data), Error::MissingActor);
    });
    test("source catalog and plan lifetime in pending slot", [] {
        Fixture f;
        std::weak_ptr<const Plan> weak;
        {
            auto authored = fixture("S2");
            weak = authored;
            f.cast(authored, f.burst());
        }
        check(!weak.expired(), "queue owns actual immutable M1 plan");
        auto result = take(f.host->advance_to(100));
        check(weak.expired() && result->after.actors[1].data.health == 42, "no runtime deep copy or report plan retention");
        Catalog catalog = game();
        auto host = take(ReferenceHost::create(catalog));
        catalog.operations.clear();
        const auto caster = take(host->add_actor(caster_data));
        const auto target = take(host->add_actor(target_data));
        auto value = fixture("S1");
        check(host->prepare(value, {caster, target, std::nullopt}).has_value(), "host catalog owns its keys");
    });
    test("accepted aim freezes world context", [] {
        Fixture f;
        Options options;
        options.pixels = PixelContext{"G0", "C7", 3, "G0", 10, 1, true};
        const auto source = disk_source(1, "multiply speed by 750 permille for 2 s");
        auto px = source;
        px.replace(px.find("250 mm"), 6, "25 px");
        auto compiled = Magic::compile(px, game(), options);
        check(compiled.has_value(), "contextual compiler accepted");
        Request request = f.burst();
        auto ticket = take(f.host->prepare(*compiled, request));
        request.aim = Point{10000, 10000};
        options.pixels->mm_numerator = 99;
        options.pixels->camera = "C9";
        auto released = take(f.host->release(ticket));
        check(released->after.pending[0].aim == Point{0, 0}, "release accepts owned world point");
        auto result = take(f.host->advance_to(100));
        check(result->after.actors[1].slow == Slow{750, 300}, "camera/source edits cannot move accepted point or radius");
    });
    test("forged foreign repeated and superseded tickets", [] {
        Fixture f;
        auto value = fixture("S1");
        auto before = f.host->snapshot();
        rejects(f.host->release(Ticket{}), Error::InvalidTicket);
        check(f.host->snapshot() == before, "forged input has no effects");
        Fixture other;
        auto ticket = take(f.host->prepare(value, f.contact()));
        rejects(other.host->release(ticket), Error::ForeignTicket);
        auto newer = take(f.host->prepare(value, f.contact()));
        rejects(f.host->release(ticket), Error::StaleTicket);
        auto copied = newer;
        take(f.host->release(newer));
        before = f.host->snapshot();
        rejects(f.host->release(copied), Error::ConsumedTicket);
        check(f.host->snapshot() == before, "copy does not duplicate use");
        auto expired_host = take(other.host->prepare(value, other.contact()));
        other.host.reset();
        Fixture recreated;
        rejects(recreated.host->release(expired_host), Error::ForeignTicket);
    });
    test("pure preview grants no ticket and discarded token is inert", [] {
        Fixture f;
        const auto value = fixture("S1");
        const auto before = f.host->snapshot();
        static_cast<void>(take(f.host->preview(value, f.contact())));
        rejects(f.host->release(Ticket{}), Error::InvalidTicket);
        auto ticket = take(f.host->prepare(value, f.contact()));
        ok(f.host->discard(ticket));
        rejects(f.host->release(ticket), Error::ConsumedTicket);
        check(f.host->snapshot() == before, "preview/discard no mana/world/action changes");
    });
    test("world revision staleness without lost prior output", [] {
        Fixture f;
        auto value = fixture("S1");
        auto ticket = take(f.host->prepare(value, f.contact()));
        ok(f.host->update_actor(f.outer, outer_data));
        const auto before = f.host->snapshot();
        rejects(f.host->release(ticket), Error::StaleTicket);
        check(f.host->snapshot() == before, "benign owner edit invalidates preparation");
        f.cast(value, f.contact());
    });
    test("live permission mana contact and willingness rechecks", [] {
        for (int mode = 0; mode < 5; ++mode) {
            Fixture f;
            auto ticket = take(f.host->prepare(fixture("S1"), f.contact()));
            auto caster = caster_data;
            auto target = target_data;
            Error expected = Error::Contact;
            if (mode == 0) {
                caster.can_cast = false;
                expected = Error::Permission;
            }
            if (mode == 1) {
                caster.mana = 9;
                expected = Error::Resources;
            }
            if (mode == 2)
                target.position = {276, 0}; // contact distance51
            if (mode == 3)
                target.willing = false;
            if (mode == 4)
                target.eligible = false;
            if (mode <= 1)
                ok(f.host->update_actor(f.caster, caster));
            else
                ok(f.host->update_actor(f.target, target));
            const auto before = f.host->snapshot();
            rejects(f.host->release(ticket), expected);
            check(f.host->snapshot() == before, "live refusal precedes mutation and staleness");
        }
    });
    test("contact exact50 and invalid actor references", [] {
        Fixture f;
        auto target = target_data;
        target.position = {275, 0};
        ok(f.host->update_actor(f.target, target));
        check(f.cast(fixture("S1"), f.contact())->after.actors[1].data.health == 62, "inclusive50mm contact");
        rejects(f.host->prepare(fixture("S1"), {{99, 1}, f.target, std::nullopt}), Error::MissingActor);
        rejects(f.host->prepare(fixture("S1"), {f.caster, ActorId{1, 999}, std::nullopt}), Error::MissingActor);
    });
    test("exact and insufficient mana with cap and zero health", [] {
        Fixture f;
        auto caster = caster_data;
        caster.mana = 10;
        ok(f.host->update_actor(f.caster, caster));
        auto target = target_data;
        target.health = 95;
        ok(f.host->update_actor(f.target, target));
        auto report = f.cast(fixture("S1"), f.contact());
        check(report->after.actors[0].data.mana == 0 && report->after.actors[1].data.health == 100, "exact cost and capped heal");
        const auto before = f.host->snapshot();
        rejects(f.host->prepare(fixture("S1"), f.contact()), Error::Resources);
        check(f.host->snapshot() == before, "insufficient funds preserved");
        Fixture lethal;
        target = target_data;
        target.health = 3;
        ok(lethal.host->update_actor(lethal.target, target));
        lethal.cast(fixture("S2"), lethal.burst());
        auto onset = take(lethal.host->advance_to(100));
        check(
            onset->after.actors[1].data.health == 0 && onset->after.actors[1].slow == Slow{750, 300},
            "preselected bundle status with clamped lethal fire"
        );
    });
    test("key plus primitive current schema authority", [] {
        Fixture f;
        auto rogue = game();
        rogue.operations[0].key = "unknown.restore";
        const auto before = f.host->snapshot();
        // An actual M1 plan can carry a supported primitive with a key foreign to this host.
        const std::string source =
            "spoken release via contact after 0 ms { bind a = willing touched actor at release ; together ordered { on a bridge Common@1 { heal 12 hp instant } ; } }";
        rejects(f.host->prepare(plan(source, rogue), f.contact()), Error::SchemaMismatch);
        check(f.host->snapshot() == before, "enum alone cannot dispatch");
        auto ticket = take(f.host->prepare(fixture("S1"), f.contact()));
        Catalog changed;
        changed.operations = {{Primitive::Fire, "game.heal"}};
        ok(f.host->replace_catalog(changed));
        auto now = f.host->snapshot();
        rejects(f.host->release(ticket), Error::SchemaMismatch);
        check(f.host->snapshot() == now, "known key with changed primitive refuses");
        changed = game();
        changed.operations[0].max_quantity = 11;
        ok(f.host->replace_catalog(changed));
        rejects(f.host->prepare(fixture("S1"), f.contact()), Error::SchemaMismatch);
    });
    test("current schema rechecked at delayed activation", [] {
        for (int mode = 0; mode < 3; ++mode) {
            Fixture f;
            f.cast(fixture("S2"), f.burst());
            auto changed = game();
            if (mode == 0)
                changed.operations[1].key = "game.new-fire";
            if (mode == 1)
                changed.operations[2].max_duration_ticks = 199;
            if (mode == 2)
                changed.burst = false;
            ok(f.host->replace_catalog(changed));
            const auto before = f.host->snapshot();
            auto error = rejects(f.host->advance_to(100), Error::SchemaMismatch);
            check(
                error.action == 1 && error.tick == 100 && f.host->snapshot() == before,
                "paid queue/resources/world preserved on hot schema change"
            );
            auto cancelled = take(f.host->cancel(error.action));
            check(!cancelled->after.pending[0].present && cancelled->after.actors[0].data.mana == 80, "cancel frees slot without refund");
        }
    });
    test("pending permission and caster generation recheck", [] {
        for (bool remove : {false, true}) {
            Fixture f;
            f.cast(fixture("S2"), f.burst());
            if (remove) {
                ok(f.host->remove_actor(f.caster));
                static_cast<void>(take(f.host->add_actor(caster_data)));
            } else {
                auto caster = caster_data;
                caster.can_cast = false;
                ok(f.host->update_actor(f.caster, caster));
            }
            const auto before = f.host->snapshot();
            auto error = rejects(f.host->advance_to(100), remove ? Error::MissingActor : Error::Permission);
            check(error.action == 1 && f.host->snapshot() == before, "current caster authority failure with action context");
        }
    });
    test("overlap health conflicts both storage and effect orders", [] {
        for (bool reverse_storage : {false, true})
            for (bool reverse_effects : {false, true}) {
                Fixture f({}, reverse_storage);
                auto source = disk_source(2, "heal 12 hp instant");
                auto insert = source.rfind("} }");
                source.insert(insert, "on b1 bridge Common@1 { fire damage 8 dp instant } ; ");
                if (reverse_effects) {
                    const auto a = source.find("heal 12 hp instant");
                    source.replace(a, 18, "fire damage 8 dp instant");
                    const auto b = source.rfind("fire damage 8 dp instant");
                    source.replace(b, 24, "heal 12 hp instant");
                }
                auto entrant = outer_data;
                entrant.position = {0, 0};
                ok(f.host->update_actor(f.outer, entrant));
                check(f.target.slot == (reverse_storage ? 2u : 1u), "logical target is created in each actual slot order");
                f.cast(plan(source), f.burst());
                const auto before = f.host->snapshot();
                rejects(f.host->advance_to(100), Error::Overlap);
                check(f.host->snapshot() == before, "cross-binding heal/fire refuses independent of effect/storage order");
            }
    });
    test("overlap speed fields across distinct bindings", [] {
        Fixture f;
        auto source = disk_source(2, "multiply speed by 750 permille for 2000 ms");
        source.insert(source.rfind("} }"), "on b1 bridge Common@1 { multiply speed by 500 permille for 1000 ms } ; ");
        f.cast(plan(source), f.burst());
        const auto before = f.host->snapshot();
        rejects(f.host->advance_to(100), Error::Overlap);
        check(f.host->snapshot() == before, "no binding-name stacking");
    });
    test("active slow refuses replacement then expires before activation", [] {
        Fixture f;
        f.cast(fixture("S3"), f.burst());
        auto before = f.host->snapshot();
        rejects(f.host->prepare(fixture("S3"), f.burst()), Error::ActiveSlow);
        check(f.host->snapshot() == before, "no stacking/refresh");
        take(f.host->advance_to(100));
        auto delayed = plan(disk_source(1, "multiply speed by 500 permille for 1000 ms"));
        f.cast(delayed, f.burst());
        auto onset = take(f.host->advance_to(200));
        check(onset->after.actors[1].slow == Slow{500, 300}, "expiry at200 precedes new activation at200");
        take(f.host->advance_to(300));
        check(f.host->snapshot().actors[1].slow == Slow{}, "replacement expires with no compounded factor");
    });
    test("whole advance rollback after earlier successful scratch action", [] {
        Fixture f;
        f.cast(plan(disk_source(1, "heal 12 hp instant")), f.burst());
        f.cast(fixture("S2"), f.burst());
        auto changed = game();
        changed.operations[1].key = "changed.fire";
        ok(f.host->replace_catalog(changed));
        const auto before = f.host->snapshot();
        auto error = rejects(f.host->advance_to(1000), Error::SchemaMismatch);
        check(error.action == 2 && f.host->snapshot() == before, "earlier scratch heal and queue removal rolled back");
        take(f.host->cancel(2));
        auto result = take(f.host->advance_to(100));
        check(
            result->after.actors[1].data.health == 62 && result->after.actors[0].data.mana == 70,
            "explicit cancel allows original paid heal once"
        );
    });
    test("exact paid delayed funds need no second debit", [] {
        Fixture f;
        auto caster = caster_data;
        caster.mana = 20;
        ok(f.host->update_actor(f.caster, caster));
        auto released = f.cast(fixture("S2"), f.burst());
        check(released->after.actors[0].data.mana == 0, "two effects paid at release exactly");
        auto onset = take(f.host->advance_to(100));
        check(
            onset->after.actors[0].data.mana == 0 && onset->after.actors[1].data.health == 42 &&
                onset->after.actors[1].slow == Slow{750, 300},
            "paid activation executes with zero remaining mana"
        );
    });
    test("active slow at delayed onset refuses complete activation", [] {
        Fixture f;
        f.cast(fixture("S2"), f.burst());
        f.cast(fixture("S3"), f.burst());
        const auto before = f.host->snapshot();
        const auto failure = rejects(f.host->advance_to(100), Error::ActiveSlow);
        check(
            failure.action == 1 && failure.tick == 100 && f.host->snapshot() == before,
            "fire, speed, clock, paid queue and funds survive delayed active-status refusal"
        );
        auto cancelled = take(f.host->cancel(1));
        check(
            cancelled->after.actors[0].data.mana == 70 && cancelled->after.actors[1].data.health == 50,
            "explicit cancellation frees pending slot without refund or partial fire"
        );
        take(f.host->advance_to(200));
        check(f.host->snapshot().actors[1].slow == Slow{}, "original accepted status expires normally");
    });
    test("deferred status quota is checked at activation", [] {
        HostLimits limits;
        limits.statuses = 0;
        Fixture f(limits);
        f.cast(fixture("S2"), f.burst());
        const auto before = f.host->snapshot();
        rejects(f.host->advance_to(100), Error::StatusLimit);
        check(f.host->snapshot() == before, "no promised future status capacity or partial fire");
        take(f.host->cancel(1));
        check(take(f.host->advance_to(100))->after.actors[0].data.mana == 80, "explicit cancelled paid action does not refund");
    });
    test("cancellation never refunds or repeats an accepted action", [] {
        Fixture f;
        auto queued = f.cast(fixture("S2"), f.burst());
        auto id = queued->events[0].action;
        auto cancelled = take(f.host->cancel(id));
        check(cancelled->after.actors[0].data.mana == 80, "release payment remains spent");
        const auto before = f.host->snapshot();
        rejects(f.host->cancel(id), Error::MissingAction);
        check(f.host->snapshot() == before, "repeated cancel no mutation");
        auto result = take(f.host->advance_to(1000));
        check(result->after.actors[1].data.health == 50 && result->after.actors[1].slow == Slow{}, "cancelled cast remains inert");
    });
    test("empty current selection has no charge and delayed empty preserves queue", [] {
        Fixture f;
        auto target = target_data;
        target.eligible = false;
        ok(f.host->update_actor(f.target, target));
        auto before = f.host->snapshot();
        rejects(f.host->prepare(fixture("S3"), f.burst()), Error::EmptySelection);
        check(f.host->snapshot() == before, "immediate empty refuses before charge");
        f.cast(fixture("S2"), f.burst());
        before = f.host->snapshot();
        rejects(f.host->advance_to(100), Error::EmptySelection);
        check(f.host->snapshot() == before, "delayed initially empty can wait for future members, refusal leaves paid queue");
        ok(f.host->update_actor(f.target, target_data));
        check(take(f.host->advance_to(100))->after.actors[1].data.health == 42, "owner explicitly fixes cause then advances");
    });
    test("engraved and ray inspect but refuse runtime", [] {
        Fixture f;
        auto before = f.host->snapshot();
        auto engraving = fixture("S5");
        check(engraving->inspection().find("item-touch") != std::string_view::npos, "S5 still inspectable");
        rejects(f.host->prepare(engraving, f.contact()), Error::UnsupportedEntry);
        std::string source(fixture("S1")->common());
        source.replace(source.find("contact"), 7, "ray");
        rejects(f.host->prepare(plan(source), f.contact()), Error::UnsupportedDelivery);
        check(f.host->snapshot() == before, "unsupported surfaces never retarget");
    });
    test("unsupported frame invalid aim null and exact plan bytes", [] {
        Fixture f;
        auto source = disk_source(1, "heal 1 hp instant", 0);
        source.replace(source.find("G0"), 2, "G1");
        rejects(f.host->prepare(plan(source), f.burst()), Error::UnsupportedBinding);
        rejects(f.host->prepare(fixture("S3"), f.burst({1'000'001, 0})), Error::InvalidAim);
        rejects(f.host->prepare({}, f.contact()), Error::NoPlan);
        auto value = fixture("S1");
        HostLimits limits;
        limits.plan_bytes = value->metrics().retained_bytes;
        Fixture exact(limits);
        exact.cast(value, exact.contact());
        --limits.plan_bytes;
        Fixture short_limit(limits);
        rejects(short_limit.host->prepare(value, short_limit.contact()), Error::PlanLimit);
    });
    test("distance extremes and valid actor limits", [] {
        Fixture f;
        auto target = target_data;
        target.position = {-1'000'000, -1'000'000};
        ok(f.host->update_actor(f.target, target));
        auto source = disk_source(1, "heal 1 hp instant", 0);
        source.replace(source.find("250 mm"), 6, "1000000 mm");
        const auto before = f.host->snapshot();
        rejects(f.host->prepare(plan(source), f.burst({1'000'000, 1'000'000})), Error::EmptySelection);
        check(f.host->snapshot() == before, "large checked squared distance does not overflow into selection");
        target.position.x = 1'000'001;
        rejects(f.host->update_actor(f.target, target), Error::InvalidActor);
        HostLimits limits;
        limits.actors = 3;
        Fixture full(limits); // complete fixture exactly fills actor quota
        rejects(full.host->add_actor(target_data), Error::ActorLimit);
    });
    test("pending exact ceiling and no debit on full queue", [] {
        Fixture f;
        auto value = plan(disk_source(1, "heal 1 hp instant"));
        for (int i = 0; i < 8; ++i)
            f.cast(value, f.burst());
        const auto before = f.host->snapshot();
        check(before.actors[0].data.mana == 20 && before.pending[7].action == 8, "eight reserved slots");
        auto ticket = take(f.host->prepare(value, f.burst()));
        rejects(f.host->release(ticket), Error::PendingLimit);
        check(f.host->snapshot() == before, "queue refusal keeps funds/actions/prior slots");
        take(f.host->cancel(1));
        f.cast(value, f.burst());
        check(f.host->snapshot().pending[0].action == 9, "freed slot gets fresh monotonic action");
        HostLimits zero;
        zero.pending = 0;
        Fixture blocked(zero);
        rejects(blocked.host->release(take(blocked.host->prepare(value, blocked.burst()))), Error::PendingLimit);
        blocked.cast(fixture("S1"), blocked.contact());
    });
    test("active status exact ceiling and one less", [] {
        for (std::size_t cap : {std::size_t{16}, std::size_t{15}, std::size_t{0}}) {
            HostLimits limits;
            limits.statuses = cap;
            auto host = take(ReferenceHost::create(game(), limits));
            ActorId caster;
            for (int i = 0; i < 16; ++i) {
                auto data = target_data;
                data.position = {};
                if (i == 0) {
                    data.can_cast = true;
                    data.mana = 100;
                }
                auto id = take(host->add_actor(data));
                if (i == 0)
                    caster = id;
            }
            const Request request{caster, std::nullopt, Point{}};
            const auto before = host->snapshot();
            if (cap == 16) {
                take(host->release(take(host->prepare(fixture("S3"), request))));
                const auto snapshot = host->snapshot();
                check(
                    std::count_if(snapshot.actors.begin(), snapshot.actors.end(), [](const auto& a) { return a.slow.until_tick == 200; }) ==
                        16,
                    "sixteen active statuses"
                );
            } else {
                rejects(host->prepare(fixture("S3"), request), Error::StatusLimit);
                check(host->snapshot() == before, "status capacity refusal atomic");
            }
        }
    });
    test("complete step target128 and selector1024 exact bounds", [] {
        for (bool selector_case : {false, true}) {
            HostLimits limits;
            auto host = take(ReferenceHost::create(game(), limits));
            ActorId caster;
            for (int i = 0; i < 16; ++i) {
                auto data = target_data;
                data.position = {};
                data.eligible = !selector_case || i == 1;
                if (i == 0) {
                    data.can_cast = true;
                    data.mana = 100;
                }
                auto id = take(host->add_actor(data));
                if (i == 0)
                    caster = id;
            }
            const auto value = plan(disk_source(selector_case ? 8 : 1, "heal 1 hp instant"));
            const Request request{caster, std::nullopt, Point{}};
            for (int i = 0; i < 8; ++i)
                take(host->release(take(host->prepare(value, request))));
            auto report = take(host->advance_to(100));
            check(report->event_count == 8, "eight activations once in ID order");
            for (std::size_t i = 0; i < 8; ++i)
                check(report->events[i].action == i + 1, "accepted ID ordering");
            check(
                selector_case ? report->usage.selector_work == 1024 : report->usage.selected_targets == 128,
                "exact complete step selector/target ceiling"
            );
            if (selector_case)
                limits.selector_work = 1023;
            else
                limits.selected_targets = 127;
            auto blocked = take(ReferenceHost::create(game(), limits));
            for (int i = 0; i < 16; ++i) {
                auto data = report->before.actors[static_cast<std::size_t>(i)].data;
                if (i == 0)
                    data.mana = 100;
                static_cast<void>(take(blocked->add_actor(data)));
            }
            for (int i = 0; i < 8; ++i)
                take(blocked->release(take(blocked->prepare(value, request))));
            const auto before = blocked->snapshot();
            rejects(blocked->advance_to(100), selector_case ? Error::SelectorLimit : Error::TargetLimit);
            check(blocked->snapshot() == before, "whole-step boundary refusal preserves eight paid queued casts");
        }
    });
    test("exact work boundary and rejected work preserves prior state", [] {
        Fixture measure;
        auto value = fixture("S2");
        measure.cast(value, measure.burst());
        auto measured = take(measure.host->advance_to(100));
        const auto work = measured->usage.work;
        HostLimits limits;
        limits.work = work;
        Fixture exact(limits);
        exact.cast(value, exact.burst());
        check(take(exact.host->advance_to(100))->usage.work == work, "exact complete step work accepted");
        --limits.work;
        Fixture blocked(limits);
        blocked.cast(value, blocked.burst());
        auto before = blocked.host->snapshot();
        rejects(blocked.host->advance_to(100), Error::WorkLimit);
        check(blocked.host->snapshot() == before, "work failure includes scratch writes and resource/queue preservation");
        std::cout << "Exact S2 activation work=" << work << '\n';
    });
    test("time exact onset expiry and range exhaustion", [] {
        HostLimits limits;
        limits.max_tick = 300;
        Fixture exact(limits);
        exact.cast(fixture("S2"), exact.burst());
        take(exact.host->advance_to(300));
        check(exact.host->snapshot().actors[1].slow == Slow{}, "onset100 expiry300 in one explicit step");
        auto before = exact.host->snapshot();
        rejects(exact.host->advance_to(301), Error::TimeLimit);
        rejects(exact.host->advance_to(299), Error::TimeLimit);
        check(exact.host->snapshot() == before, "forward limit and rewind refusal");
        Fixture overflow(limits);
        take(overflow.host->advance_to(1));
        overflow.cast(fixture("S2"), overflow.burst());
        before = overflow.host->snapshot();
        rejects(overflow.host->advance_to(101), Error::TimeLimit);
        check(overflow.host->snapshot() == before, "status expiry checked before mutation");
        auto fire = plan(disk_source(1, "fire damage 1 dp instant"));
        HostLimits final_time;
        final_time.max_tick = 100;
        Fixture final(final_time);
        final.cast(fire, final.burst());
        check(
            take(final.host->advance_to(100))->after.actors[1].data.health == 49, "activation does not add onset again during validation"
        );
    });
    test("action ticket revision and generation exhaustion", [] {
        HostLimits limits;
        limits.max_action = 1;
        Fixture action(limits);
        action.cast(fixture("S1"), action.contact());
        auto before = action.host->snapshot();
        rejects(action.host->release(take(action.host->prepare(fixture("S1"), action.contact()))), Error::ActionLimit);
        check(action.host->snapshot() == before, "no action ID reuse");
        limits = {};
        limits.max_ticket = 1;
        Fixture ticket(limits);
        auto first = take(ticket.host->prepare(fixture("S1"), ticket.contact()));
        rejects(ticket.host->prepare(fixture("S1"), ticket.contact()), Error::TicketLimit);
        take(ticket.host->release(first));
        limits = {};
        limits.max_revision = 3;
        Fixture revision(limits);
        before = revision.host->snapshot();
        rejects(revision.host->release(take(revision.host->prepare(fixture("S1"), revision.contact()))), Error::RevisionLimit);
        check(revision.host->snapshot() == before, "revision exhaustion does not debit");
        limits = {};
        limits.actors = 1;
        limits.max_generation = 1;
        auto host = take(ReferenceHost::create(game(), limits));
        auto id = take(host->add_actor(target_data));
        ok(host->remove_actor(id));
        before = host->snapshot();
        rejects(host->add_actor(target_data), Error::GenerationLimit);
        check(host->snapshot() == before, "generation exhaustion does not resurrect identity");
    });
    test("zero quotas and fixed ceilings", [] {
        HostLimits limits;
        limits.work = 0;
        auto host = take(ReferenceHost::create(game(), limits));
        rejects(host->add_actor(target_data), Error::WorkLimit);
        limits = {};
        limits.actors = 0;
        host = take(ReferenceHost::create(game(), limits));
        rejects(host->add_actor(target_data), Error::ActorLimit);
        for (int i = 0; i < 7; ++i) {
            limits = {};
            std::array<std::size_t*, 7> values{&limits.actors,        &limits.pending, &limits.statuses,  &limits.selected_targets,
                                               &limits.selector_work, &limits.work,    &limits.plan_bytes};
            ++*values[static_cast<std::size_t>(i)];
            rejects(ReferenceHost::create(game(), limits), Error::InvalidLimits);
        }
        for (int i = 0; i < 5; ++i) {
            limits = {};
            std::array<std::uint64_t*, 5> values{
                &limits.max_tick, &limits.max_action, &limits.max_ticket, &limits.max_revision, &limits.max_generation
            };
            ++*values[static_cast<std::size_t>(i)];
            rejects(ReferenceHost::create(game(), limits), Error::InvalidLimits);
        }
        auto invalid = game();
        invalid.operations[1].key = invalid.operations[0].key;
        rejects(ReferenceHost::create(invalid), Error::InvalidCatalog);
    });
    std::cout << "Fixed object bytes: host=" << sizeof(ReferenceHost) << " snapshot=" << sizeof(Snapshot)
              << " observation=" << sizeof(Observation) << '\n';
    std::cout << passed << " reference host cases passed; " << failed << " failed.\n";
    return failed == 0 ? 0 : 1;
}
