#include "drainage-checks.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

namespace DrainageAllocation {
    thread_local bool denied = false;
}
void* operator new(std::size_t size) {
    if (DrainageAllocation::denied) {
        throw std::bad_alloc();
    }
    if (void* memory = std::malloc(size == 0 ? 1 : size)) {
        return memory;
    }
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) {
    return ::operator new(size);
}
void operator delete(void* p) noexcept {
    std::free(p);
}
void operator delete[](void* p) noexcept {
    std::free(p);
}
void operator delete(void* p, std::size_t) noexcept {
    std::free(p);
}
void operator delete[](void* p, std::size_t) noexcept {
    std::free(p);
}

namespace {
    using namespace ProcGen;
    constexpr auto budget = drainage_hard_limits;
    constexpr auto high = std::numeric_limits<Height>::max(), low = std::numeric_limits<Height>::min();
    std::size_t assertions = 0;
    void check(bool ok, const char* expression, int line) {
        ++assertions;
        if (!ok) {
            throw std::runtime_error("line " + std::to_string(line) + ": " + expression);
        }
    }
#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)
    struct Deny {
        bool previous = DrainageAllocation::denied;
        Deny() { DrainageAllocation::denied = true; }
        ~Deny() { DrainageAllocation::denied = previous; }
    };
    template <class T> void error_is(const DrainageResult<T>& result, FailureKind kind, DrainageReason reason) {
        CHECK(!result);
        CHECK(result.error().kind == kind);
        CHECK(result.error().reason == reason);
    }
    Domain domain(Index x, Index y) {
        return Domain::checked(std::array<Index, 2>{x, y}).value();
    }
    bool same(const Drainage& a, const Drainage& b) {
        return a.descriptor().domain == b.descriptor().domain && a.descriptor().options == b.descriptor().options &&
               std::ranges::equal(a.source_heights(), b.source_heights()) && std::ranges::equal(a.cells(), b.cells()) &&
               a.measurements() == b.measurements();
    }
    Drainage make(Domain d, std::span<const Height> heights, std::span<const Index> outlets = {}) {
        auto result = analyze_drainage({d, heights, outlets}, budget).value();
        ProcGenDrainageChecks::inspect({d, heights, outlets}, result);
        const auto needed = drainage_requirements(d, outlets.size()).value();
        CHECK(result.work() == needed.minimum_work + 2 * result.measurements().links);
        CHECK(result.work() <= needed.maximum_work);
        return result;
    }
    void literal() {
        const std::array<Height, 6> h{9, 6, 3, 8, 5, 0};
        const std::array<Index, 1> out{5};
        const auto result = make(domain(3, 2), h, out);
        const std::array<Index, 6> receivers{1, 2, 5, 4, 5, no_receiver}, contributions{1, 2, 3, 1, 2, 6};
        for (Index i = 0; i < 6; ++i) {
            const auto c = result.cell(i).value();
            CHECK(c.receiver == receivers[static_cast<std::size_t>(i)]);
            CHECK(c.terminal == 5 && c.contributing == contributions[static_cast<std::size_t>(i)]);
        }
        CHECK((result.measurements() == DrainageMeasurements{6, 5, 1, 0, 6, 0, 6, 6}));
        const std::array<Height, 9> fork{8, 7, 8, 6, 5, 6, 8, 4, 8};
        const std::array<Index, 1> goal{7};
        const auto branched = make(domain(3, 3), fork, goal);
        const std::array<Index, 9> expected{1, 1, 1, 2, 6, 2, 1, 9, 1};
        for (Index i = 0; i < 9; ++i) {
            CHECK(branched.cell(i)->contributing == expected[static_cast<std::size_t>(i)]);
            CHECK(branched.cell(i)->terminal == 7);
        }
        CHECK((branched.measurements() == DrainageMeasurements{9, 8, 1, 0, 9, 0, 9, 9}));
    }
    void terminals() {
        const std::array<Height, 9> bowl{9, 9, 9, 9, 0, 9, 9, 9, 9};
        const std::array<Index, 1> out{0};
        const auto b = make(domain(3, 3), bowl, out);
        CHECK((b.measurements() == DrainageMeasurements{9, 4, 1, 4, 1, 8, 5, 5}));
        CHECK(b.cell(4)->kind == TerminalKind::sink && b.cell(4)->contributing == 5);
        CHECK(b.cell(2)->terminal == 2 && b.cell(2)->kind == TerminalKind::sink);
        const std::array<Height, 4> plateau{2, 2, 2, 2};
        const std::array<Index, 1> corner{3};
        const auto p = make(domain(2, 2), plateau, corner);
        CHECK((p.measurements() == DrainageMeasurements{4, 0, 1, 3, 1, 3, 1, 1}));
        for (Index i = 0; i < 4; ++i) {
            CHECK(p.cell(i)->receiver == no_receiver && p.cell(i)->terminal == i);
        }
        const std::array<Height, 3> slope{5, 4, 3};
        const std::array<Index, 2> ends{2, 0};
        const auto stopped = make(domain(3, 1), slope, ends);
        CHECK(stopped.cell(0)->kind == TerminalKind::outlet && stopped.cell(0)->receiver == no_receiver);
        CHECK(stopped.cell(1)->terminal == 2 && stopped.cell(2)->contributing == 2);
        CHECK(stopped.measurements().outlet_contributing == 3 && stopped.measurements().sinks == 0);
        const auto no_outlets = make(domain(1, 3), slope);
        CHECK(no_outlets.cell(2)->kind == TerminalKind::sink && no_outlets.cell(2)->contributing == 3);
        const std::array<Height, 1> singleton{-9};
        CHECK(make(domain(1, 1), singleton).cell(0)->kind == TerminalKind::sink);
        CHECK(make(domain(1, 1), singleton, out).cell(0)->kind == TerminalKind::outlet);
    }
    void ties() {
        const std::array<Height, 9> h{20, 1, 20, 1, 10, 1, 20, 1, 20};
        const auto a = make(domain(3, 3), h);
        CHECK(a.cell(4)->receiver == 1); // y- wins over x- by smaller index
        const std::array<Height, 4> diagonal{9, 0, 1, 8};
        const auto b = make(domain(2, 2), diagonal);
        CHECK(b.cell(2)->receiver == no_receiver); // index1 is diagonal; no row wrap
        const std::array<Index, 3> x{8, 0, 4}, y{4, 8, 0};
        CHECK(same(make(domain(3, 3), h, x), make(domain(3, 3), h, y)));
        CHECK(same(a, make(domain(3, 3), h)));
        auto larger = budget;
        larger.work = std::numeric_limits<Index>::max();
        CHECK(same(a, analyze_drainage({domain(3, 3), h, {}}, larger).value()));
    }
    void extremes() {
        for (auto d : {domain(3, 1), domain(1, 3)}) {
            const std::array<Height, 3> h{high, low, high};
            const auto r = make(d, h);
            CHECK(r.cell(0)->receiver == 1 && r.cell(2)->receiver == 1);
            CHECK(r.cell(1)->contributing == 3 && r.cell(1)->kind == TerminalKind::sink);
            CHECK(static_cast<std::int64_t>(high) - low == INT64_C(4294967295));
        }
        for (Height height : {low, high}) {
            const std::array<Height, 1> h{height};
            CHECK(make(domain(1, 1), h).cell(0)->contributing == 1);
        }
    }
    void exhaustive() {
        const auto d = domain(3, 2);
        const std::array<Index, 1> first{0}, last{5};
        Index examined = 0;
        for (Index code = 0; code < 729; ++code) {
            std::array<Height, 6> h{};
            auto digits = code;
            for (auto& v : h) {
                v = static_cast<Height>(digits % 3) - 1;
                digits /= 3;
            }
            for (auto outlets : {std::span<const Index>{}, std::span<const Index>{first}, std::span<const Index>{last}}) {
                const auto r = make(d, h, outlets);
                CHECK(r.measurements().outlet_contributing + r.measurements().sink_contributing == 6);
                ++examined;
            }
        }
        CHECK(examined == 2187);
    }
    void generated() {
        for (Index seed : {0U, 1U, 42U, 991U}) {
            for (Index spacing : {3U, 6U}) {
                const auto field =
                    generate_elevation(std::array<Index, 2>{17, 11}, {seed, 0x67656f6772617068, 7}, {spacing, -80, 120}, budget).value();
                const auto copy = field;
                const std::array<Index, 2> out{0, 186};
                const auto r = make(field.descriptor().domain, field.heights(), out);
                CHECK(r.measurements().cells == 187 && r.measurements().outlets == 2);
                CHECK(std::ranges::equal(field.heights(), copy.heights()) && field.descriptor() == copy.descriptor());
                CHECK(std::ranges::equal(r.source_heights(), field.heights()));
            }
        }
    }
    void invalid() {
        const auto d = domain(2, 2);
        const std::array<Height, 4> h{3, 2, 1, 0};
        const auto three = Domain::checked(std::array<Index, 3>{2, 2, 1}).value();
        {
            Deny deny;
            error_is(analyze_drainage({three, h, {}}, budget), FailureKind::invalid_input, DrainageReason::dimensions);
        }
        {
            Deny deny;
            error_is(
                analyze_drainage({d, std::span<const Height>(h).first(3), {}}, budget), FailureKind::invalid_input,
                DrainageReason::height_count
            );
        }
        {
            Deny deny;
            error_is(analyze_drainage({domain(3, 1), h, {}}, budget), FailureKind::invalid_input, DrainageReason::height_count);
        }
        for (auto out : {std::array<Index, 2>{0, 0}, {1, 4}, {0, no_receiver}}) {
            Deny deny;
            error_is(analyze_drainage({d, h, out}, budget), FailureKind::invalid_input, DrainageReason::outlets);
        }
        {
            Deny deny;
            error_is(drainage_requirements(d, 5), FailureKind::invalid_input, DrainageReason::outlets);
        }
        {
            Deny deny;
            error_is(
                analyze_drainage({d, h, {}}, budget, {static_cast<DrainageAlgorithm>(0), 1}), FailureKind::unsupported,
                DrainageReason::algorithm
            );
        }
        for (auto version : {0U, 2U}) {
            Deny deny;
            error_is(
                analyze_drainage({d, h, {}}, budget, {DrainageAlgorithm::steepest_cardinal, version}), FailureKind::unsupported,
                DrainageReason::version
            );
        }
        const auto huge = domain(std::numeric_limits<Index>::max(), 1);
        {
            Deny deny;
            error_is(drainage_requirements(huge, 0), FailureKind::invalid_input, DrainageReason::overflow);
        }
        const auto bytes = domain(std::numeric_limits<Index>::max() / 48 + 1, 1);
        {
            Deny deny;
            error_is(drainage_requirements(bytes, 0), FailureKind::invalid_input, DrainageReason::overflow);
        }
        CHECK(!Domain::checked({}));
        CHECK(!Domain::checked(std::array<Index, 2>{0, 1}));
        CHECK(!Domain::checked(std::array<Index, 2>{std::numeric_limits<Index>::max(), 2}));
        const auto r = make(d, h);
        for (Index index : {Index{4}, no_receiver}) {
            Deny deny;
            error_is(r.cell(index), FailureKind::invalid_input, DrainageReason::index);
        }
    }
    void limits() {
        const auto d = domain(3, 2);
        const std::array<Height, 6> h{9, 6, 3, 8, 5, 0};
        const std::array<Index, 1> out{5};
        const DrainageInput input{d, h, out};
        const auto original = make(d, h, out), snapshot = original;
        const auto needed = drainage_requirements(d, 1).value();
        CHECK(needed.cells == 6 && needed.storage_bytes == 12576 && needed.minimum_work == 1100 && needed.maximum_work == 1110);
        CHECK(original.work() == 1110);
        const CaveBudgets exact{6, 12576, 1110};
        CHECK(same(original, analyze_drainage(input, exact).value()));
        for (auto reason : {DrainageReason::cells, DrainageReason::storage, DrainageReason::work}) {
            auto short_budget = exact;
            if (reason == DrainageReason::cells) {
                --short_budget.cells;
            }
            if (reason == DrainageReason::storage) {
                --short_budget.storage_bytes;
            }
            if (reason == DrainageReason::work) {
                short_budget.work = needed.minimum_work - 1;
            }
            Deny deny;
            error_is(analyze_drainage(input, short_budget), FailureKind::budget_refused, reason);
        }
        for (Index work = needed.minimum_work; work < original.work(); ++work) {
            auto shorter = exact;
            shorter.work = work;
            error_is(analyze_drainage(input, shorter), FailureKind::budget_refused, DrainageReason::work);
            CHECK(same(original, snapshot) && h[0] == 9 && out[0] == 5);
        }
        const std::array<Index, 6> all{5, 4, 3, 2, 1, 0};
        const auto stopped = make(d, h, all);
        const auto plan = drainage_requirements(d, 6).value();
        CHECK(stopped.work() == plan.minimum_work && plan.minimum_work == plan.maximum_work);
        CHECK(analyze_drainage({d, h, all}, {6, plan.storage_bytes, plan.minimum_work}));
        const auto too_large = domain(drainage_hard_limits.cells + 1, 1);
        {
            Deny deny;
            error_is(analyze_drainage({too_large, {}, {}}, budget), FailureKind::budget_refused, DrainageReason::cells);
        }
        const auto maximum = domain(256, 256);
        const std::vector<Height> flat(static_cast<std::size_t>(maximum.cells()), 0);
        const auto accepted = analyze_drainage({maximum, flat, {}}, budget).value();
        CHECK(accepted.measurements().sinks == 65536 && accepted.measurements().links == 0);
        CHECK(accepted.work() == drainage_requirements(maximum, 0)->minimum_work);
        CHECK(accepted.measurements().sink_contributing == 65536);
    }
    void ownership() {
        std::array<Height, 6> h{9, 6, 3, 8, 5, 0};
        std::array<Index, 1> out{5};
        const auto r = make(domain(3, 2), h, out), snapshot = r;
        CHECK(r.source_heights().data() != h.data());
        CHECK(r.cells().data() != snapshot.cells().data());
        CHECK(r.source_heights().data() != snapshot.source_heights().data());
        h.fill(0);
        out[0] = 0;
        CHECK(same(r, snapshot) && r.source_heights()[0] == 9 && r.cell(5)->kind == TerminalKind::outlet);
        bool propagated = false;
        try {
            Deny deny;
            static_cast<void>(analyze_drainage({domain(3, 2), h, out}, budget));
        } catch (const std::bad_alloc&) {
            propagated = true;
        }
        CHECK(propagated && same(r, snapshot));
        auto refused = budget;
        refused.storage_bytes = 0;
        error_is(analyze_drainage({domain(3, 2), h, out}, refused), FailureKind::budget_refused, DrainageReason::storage);
        CHECK(same(r, snapshot));
        const auto lifetime = [] {
            const std::array<Height, 6> heights{9, 6, 3, 8, 5, 0};
            const std::array<Index, 1> outlets{5};
            return analyze_drainage({domain(3, 2), heights, outlets}, budget).value();
        }();
        CHECK(same(lifetime, r));
        auto copy = r;
        copy = make(domain(3, 2), h, out);
        Drainage moved = std::move(copy);
        CHECK(moved.cell(0)->kind == TerminalKind::outlet && same(r, snapshot));
        const auto replay =
            analyze_drainage(
                {r.descriptor().domain, r.source_heights(), std::array<Index, 1>{5}}, r.descriptor().budgets, r.descriptor().options
            )
                .value();
        CHECK(same(replay, r));
    }
}
int main(int argc, char** argv) {
    struct Case {
        std::string_view name;
        void (*run)();
    };
    const std::array<Case, 9> cases{
        {{"drainage_literal", literal},
         {"drainage_terminals", terminals},
         {"drainage_ties", ties},
         {"drainage_extremes", extremes},
         {"drainage_exhaustive", exhaustive},
         {"drainage_generated", generated},
         {"drainage_invalid", invalid},
         {"drainage_limits", limits},
         {"drainage_ownership", ownership}}
    };
    const std::string_view selection = argc == 2 ? argv[1] : "all";
    if (argc > 2) {
        std::cerr << "Expected one drainage case name or all\n";
        return 2;
    }
    std::size_t passed = 0, failed = 0;
    for (const auto& test : cases) {
        if (selection != "all" && selection != test.name) {
            continue;
        }
        const auto before = assertions;
        try {
            test.run();
            ++passed;
            std::cout << "PASS " << test.name << ": " << assertions - before << " assertions\n";
        } catch (const std::exception& error) {
            ++failed;
            std::cerr << "FAIL " << test.name << ": " << error.what() << '\n';
        }
    }
    std::cout << "Drainage cases: " << passed << " passed, " << failed << " failed, 0 skipped; " << assertions << " assertions\n";
    return passed != 0 && failed == 0 ? 0 : 1;
}
