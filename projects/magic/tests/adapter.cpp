#define MAGIC_ADAPTER_NO_MAIN
#include "../src/adapter-demo.cpp"

#include <functional>
#include <cstdlib>
#include <new>

namespace AllocationFault {
    int remaining = -1;
}
void* operator new(std::size_t size) {
    if (AllocationFault::remaining == 0) {
        AllocationFault::remaining = -1;
        throw std::bad_alloc();
    }
    if (AllocationFault::remaining > 0)
        --AllocationFault::remaining;
    if (auto* memory = std::malloc(size == 0 ? 1 : size))
        return memory;
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

namespace {
    namespace C = Magic::Contact;
    void check(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }
    template<class T> void refuses(const std::expected<T, C::Error>& value, C::Error expected) {
        check(!value && value.error() == expected, "wrong refusal");
    }
    struct Fixture {
        Party::Aggregate owner;
        Party::Adapter adapter{owner};
        C::Runtime runtime{adapter};
        Magic::PlanHandle plan = Party::spell();
        C::Ticket prepare() { return Party::take(runtime.prepare(plan, Party::request(owner))); }
    };
}
int main() {
    int failures = 0;
    auto test = [&](const char* name, const std::function<void()>& run) {
        try { run(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) {
            ++failures;
            std::cerr << "FAIL " << name << ": " << error.what() << '\n';
        }
    };
    test("literal owned result", [] {
        C::Result report;
        {
            Fixture f;
            const auto before = f.owner.state;
            auto ticket = f.prepare();
            check(f.owner.state == before, "prepare mutated owner");
            f.plan.reset(); // only the admitted runtime retains the compiled source/meaning
            report = Party::take(f.runtime.release(ticket));
            check(f.owner.state.members[0].focus == 18 && f.owner.state.members[1].vitality == 40 &&
                  f.owner.state.casts == 1 && f.owner.state.revision == 2, "literal aggregate mismatch");
            refuses(f.runtime.release(ticket), C::Error::ConsumedTicket);
            f.plan.reset();
            f.owner.catalog.operations.clear();
        }
        check(report->health == 40 && report->resource == 18 && report->before.health == 35 &&
              report->before.resource == 21, "report borrowed destroyed owner/source");
    });
    test("ticket authority and supersession", [] {
        Fixture f, other;
        refuses(f.runtime.release({}), C::Error::InvalidTicket);
        auto first = f.prepare();
        auto second = f.prepare();
        const auto before = f.owner.state;
        refuses(other.runtime.release(second), C::Error::ForeignTicket);
        refuses(f.runtime.release(first), C::Error::StaleTicket);
        check(f.owner.state == before, "invalid ticket mutated owner");
        static_cast<void>(Party::take(f.runtime.release(second)));
        C::Ticket dead;
        { C::Runtime temporary(f.adapter); dead = Party::take(temporary.prepare(f.plan, Party::request(f.owner))); }
        C::Runtime replacement(f.adapter);
        refuses(replacement.release(dead), C::Error::ForeignTicket);
    });
    test("live refusals preserve current world", [] {
        for (int gate = 0; gate != 7; ++gate) {
            Fixture f;
            auto ticket = f.prepare();
            C::Error error = C::Error::Contact;
            switch (gate) {
                case 0: f.owner.state.permission = false; error = C::Error::Permission; break;
                case 1: f.owner.state.touching = false; break;
                case 2: f.owner.state.willing = false; break;
                case 3: f.owner.state.eligible = false; break;
                case 4: f.owner.state.members[1].living = false; break;
                case 5: f.owner.state.members[0].focus = 2; error = C::Error::Resources; break;
                case 6: ++f.owner.state.members[1].generation; error = C::Error::MissingEntity; break;
            }
            ++f.owner.state.revision;
            const auto edited = f.owner.state;
            refuses(f.runtime.release(ticket), error);
            check(f.owner.state == edited, "failed cast rolled back owner edit");
        }
    });
    test("revision discard and serial ceiling", [] {
        Fixture f;
        auto ticket = f.prepare();
        ++f.owner.state.revision;
        const auto edited = f.owner.state;
        refuses(f.runtime.release(ticket), C::Error::StaleTicket);
        check(f.owner.state == edited, "stale revision mutated current world");
        check(f.runtime.discard(ticket).has_value(), "discard failed admission");
        refuses(f.runtime.release(ticket), C::Error::ConsumedTicket);
        C::Limits limits;
        limits.tickets = 1;
        C::Runtime bounded(f.adapter, limits);
        const auto only = Party::take(bounded.prepare(f.plan, Party::request(f.owner)));
        refuses(bounded.prepare(f.plan, Party::request(f.owner)), C::Error::TicketLimit);
        static_cast<void>(Party::take(bounded.release(only)));
        check(f.owner.state.casts == 1, "ticket cap consumed earlier admission");
    });
    test("revision and schema rechecked", [] {
        Fixture f;
        auto ticket = f.prepare();
        ++f.owner.state.schema;
        const auto edited = f.owner.state;
        refuses(f.runtime.release(ticket), C::Error::StaleTicket);
        check(f.owner.state == edited, "schema rejection mutated world");
        f.owner.catalog.operations[0].key = "other.heal";
        refuses(f.runtime.release(ticket), C::Error::Schema);
        f.owner.catalog.operations[0].key = "game.heal";
        f.owner.catalog.operations[0].primitive = Magic::Primitive::Fire;
        refuses(f.runtime.release(ticket), C::Error::Schema);
        f.owner.catalog.operations[0].primitive = Magic::Primitive::Heal;
        f.owner.catalog.operations[0].max_quantity = 11;
        refuses(f.runtime.release(ticket), C::Error::Schema);
    });
    test("candidate failure and retry", [] {
        Fixture f;
        const auto ticket = f.prepare();
        const auto before = f.owner.state;
        f.adapter.fail_preparation = true;
        refuses(f.runtime.release(ticket), C::Error::Preparation);
        check(f.owner.state == before, "candidate preparation mutated owner");
        f.adapter.fail_preparation = false;
        f.adapter.throw_preparation = true;
        bool allocation_failed = false;
        try { static_cast<void>(f.runtime.release(ticket)); }
        catch (const std::bad_alloc&) { allocation_failed = true; }
        check(allocation_failed && f.owner.state == before, "throwing preparation mutated owner");
        f.adapter.throw_preparation = false;
        static_cast<void>(Party::take(f.runtime.release(ticket)));
        check(f.owner.state.casts == 1, "failure consumed valid ticket");
    });
    test("allocation before adoption", [] {
        for (const int allocation : {0, 1}) {
            Fixture f;
            const auto ticket = f.prepare();
            const auto before = f.owner.state;
            // First allocation owns the candidate; second owns the result.
            AllocationFault::remaining = allocation;
            bool failed = false;
            try { static_cast<void>(f.runtime.release(ticket)); }
            catch (const std::bad_alloc&) { failed = true; }
            AllocationFault::remaining = -1;
            check(failed && f.owner.state == before, "allocation failure partially adopted cast");
            static_cast<void>(Party::take(f.runtime.release(ticket)));
            check(f.owner.state.casts == 1, "allocation failure consumed ticket");
        }
    });
    test("late rejection preserves owner edit", [] {
        Fixture f;
        const auto ticket = f.prepare();
        f.adapter.reject_late = true;
        refuses(f.runtime.release(ticket), C::Error::StaleTicket);
        check(f.owner.state.members[0].vitality == 29 && f.owner.state.revision == 2 &&
              f.owner.state.members[0].focus == 21 && f.owner.state.members[1].vitality == 35 &&
              f.owner.state.casts == 0, "late rejection partially adopted cast");
    });
    test("exact budgets and one short", [] {
        Fixture f;
        auto request = Party::request(f.owner);
        const auto evaluation = Party::take(C::evaluate(f.plan, request, f.adapter));
        C::Limits limits;
        check(evaluation.work == 40, "literal S1 work tariff");
        limits.work = 40;
        limits.plan_bytes = f.plan->metrics().retained_bytes;
        limits.result_bytes = sizeof(C::Change);
        C::Runtime exact(f.adapter, limits);
        const auto ticket = Party::take(exact.prepare(f.plan, request));
        const auto before = f.owner.state;
        --limits.work;
        C::Runtime short_work(f.adapter, limits);
        refuses(short_work.prepare(f.plan, request), C::Error::WorkLimit);
        ++limits.work;
        --limits.plan_bytes;
        C::Runtime short_plan(f.adapter, limits);
        refuses(short_plan.prepare(f.plan, request), C::Error::PlanLimit);
        ++limits.plan_bytes;
        --limits.result_bytes;
        C::Runtime short_result(f.adapter, limits);
        refuses(short_result.prepare(f.plan, request), C::Error::ResultLimit);
        ++limits.result_bytes;
        limits.tickets = 0;
        C::Runtime no_tickets(f.adapter, limits);
        refuses(no_tickets.prepare(f.plan, request), C::Error::TicketLimit);
        check(f.owner.state == before, "budget rejection mutated owner");
        static_cast<void>(Party::take(exact.release(ticket)));
    });
    test("failed prepare preserves previous admission", [] {
        Fixture f;
        const auto ticket = f.prepare();
        refuses(f.runtime.prepare({}, Party::request(f.owner)), C::Error::NoPlan);
        static_cast<void>(Party::take(f.runtime.release(ticket)));
        check(f.owner.state.casts == 1, "failed prepare superseded valid ticket");
    });
    test("unsupported forms refuse unpaid", [] {
        Fixture f;
        Magic::Catalog catalog;
        catalog.operations = {{Magic::Primitive::Heal, "game.heal"}, {Magic::Primitive::Fire, "game.fire"},
                              {Magic::Primitive::Slow, "game.slow"}};
        const auto before = f.owner.state;
        for (const std::string name : {"S2", "S3", "S4", "S5", "mixed-schools"}) {
            std::ifstream input(std::string(MAGIC_FIXTURE_DIR) + "/" + name + ".verse");
            check(input.good(), "fixture opens");
            const std::string source(std::istreambuf_iterator<char>(input), {});
            const auto plan = Party::take(Magic::compile(source, catalog));
            refuses(f.runtime.prepare(plan, Party::request(f.owner)), C::Error::Unsupported);
            check(f.owner.state == before, "unsupported form mutated owner");
        }
    });
    test("contact composition refuses unpaid", [] {
        Fixture f;
        Magic::Catalog catalog;
        catalog.operations = {{Magic::Primitive::Heal, "game.heal"}, {Magic::Primitive::Fire, "game.fire"}};
        const std::string prefix = "spoken release via contact after 0 ms { bind hand = willing touched actor at release ; together ordered { ";
        const auto fire = Party::take(Magic::compile(prefix + "on hand bridge Common@1 { fire damage 8 dp instant } ; } }", catalog));
        const auto together = Party::take(Magic::compile(prefix +
            "on hand bridge Common@1 { heal 12 hp instant } ; on hand bridge Common@1 { fire damage 8 dp instant } ; } }", catalog));
        const auto two_bindings = Party::take(Magic::compile(
            "spoken release via contact after 0 ms { bind a = willing touched actor at release ; "
            "bind b = willing touched actor at release ; together ordered { on a bridge Common@1 { heal 12 hp instant } ; } }", catalog));
        const auto before = f.owner.state;
        refuses(f.runtime.prepare(fire, Party::request(f.owner)), C::Error::Unsupported);
        refuses(f.runtime.prepare(together, Party::request(f.owner)), C::Error::Unsupported);
        refuses(f.runtime.prepare(two_bindings, Party::request(f.owner)), C::Error::Unsupported);
        check(f.owner.state == before, "unsupported contact composition mutated owner");
    });
    test("resource and generation boundaries", [] {
        Fixture f;
        f.owner.state.members[0].focus = 3;
        const auto ticket = f.prepare();
        static_cast<void>(Party::take(f.runtime.release(ticket)));
        check(f.owner.state.members[0].focus == 0, "exact resource price");
        refuses(f.runtime.prepare(f.plan, Party::request(f.owner)), C::Error::Resources);
        f.owner.state.members[0].focus = 21;
        f.owner.state.revision = std::numeric_limits<std::uint64_t>::max();
        const auto last = f.prepare();
        const auto before = f.owner.state;
        refuses(f.runtime.release(last), C::Error::RevisionLimit);
        check(f.owner.state == before, "revision overflow mutation");
    });
    return failures == 0 ? 0 : 1;
}
