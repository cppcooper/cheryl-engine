#include "spill-checks.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

namespace SpillAllocation {
    thread_local bool denied = false;
}
void* operator new(std::size_t size) {
    if (SpillAllocation::denied) {
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
    constexpr auto budget = spill_hard_limits;
    constexpr auto low = std::numeric_limits<Height>::min(), high = std::numeric_limits<Height>::max();
    std::size_t assertions = 0;
    void check(bool ok, const char* expression, int line) {
        ++assertions;
        if (!ok) {
            throw std::runtime_error("line " + std::to_string(line) + ": " + expression);
        }
    }
#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)
    struct Deny {
        bool previous = SpillAllocation::denied;
        Deny() { SpillAllocation::denied = true; }
        ~Deny() { SpillAllocation::denied = previous; }
    };
    template <class T> void error_is(const SpillResult<T>& result, FailureKind kind, SpillReason reason) {
        CHECK(!result);
        CHECK(result.error().kind == kind);
        CHECK(result.error().reason == reason);
    }
    Domain domain(Index x, Index y) {
        return Domain::checked(std::array<Index, 2>{x, y}).value();
    }
    bool same(const Spill& a, const Spill& b) {
        return a.descriptor().domain == b.descriptor().domain && a.descriptor().options == b.descriptor().options &&
               std::ranges::equal(a.cells(), b.cells()) && a.measurements() == b.measurements();
    }
    Spill make(Domain d, std::span<const Height> h, std::span<const Index> out) {
        auto r = analyze_spill({d, h, out}, budget).value();
        ProcGenSpillChecks::inspect({d, h, out}, r);
        const auto needed = spill_requirements(d, out.size(), budget.queue_entries).value();
        CHECK(r.usage().work == needed.minimum_work + r.usage().heap_extra_work);
        CHECK(r.usage().work <= needed.maximum_work);
        CHECK(r.measurements().volume <= needed.maximum_volume);
        return r;
    }
    void literal() {
        const std::array<Height, 9> bowl{5, 5, 5, 5, -2, 5, 5, 5, 5};
        const std::array<Index, 1> out{0};
        const auto r = make(domain(3, 3), bowl, out);
        for (Index i = 0; i < 9; ++i) {
            CHECK(r.cell(i)->effective == 5 && r.cell(i)->depth == (i == 4 ? 7 : 0));
        }
        CHECK((r.measurements() == SpillMeasurements{9, 1, 1, 7, 7, 5, 5}));
        const std::array<Height, 21> nested{9, 9, 9, 9, 9, 9, 9, 0, 6, -2, 3, -4, 8, 1, 9, 9, 9, 9, 9, 9, 9};
        const std::array<Index, 2> outlets{7, 13};
        const auto n = make(domain(7, 3), nested, outlets);
        for (Index i = 0; i < 21; ++i) {
            CHECK(n.cell(i)->effective == ((i >= 9 && i <= 11) ? 6 : nested[static_cast<std::size_t>(i)]));
        }
        CHECK(n.cell(9)->depth == 8 && n.cell(10)->depth == 3 && n.cell(11)->depth == 10);
        CHECK(n.cell(11)->outlet == 7 && n.cell(12)->outlet == 13);
        CHECK((n.measurements() == SpillMeasurements{21, 2, 3, 10, 21, 0, 9}));
    }
    void saddles() {
        const std::array<Height, 5> h{0, 5, -3, 5, 0};
        const std::array<Index, 2> out{0, 4}, reversed{4, 0};
        const auto r = make(domain(5, 1), h, out);
        CHECK(r.cell(2)->effective == 5 && r.cell(2)->depth == 8 && r.cell(2)->outlet == 0);
        const std::array<Index, 5> parents{no_receiver, 0, 1, 4, no_receiver}, ranks{0, 2, 3, 4, 1};
        for (Index i = 0; i < 5; ++i) {
            CHECK(r.cell(i)->parent == parents[static_cast<std::size_t>(i)]);
            CHECK(r.cell(i)->rank == ranks[static_cast<std::size_t>(i)]);
        }
        CHECK(same(r, make(domain(5, 1), h, reversed)));
        CHECK(same(r, make(domain(5, 1), h, out)));
        CHECK(r.measurements().volume == 8);
        const std::array<Height, 4> nonwrap{0, 9, -1, 8};
        const std::array<Index, 1> corner{1};
        const auto b = make(domain(2, 2), nonwrap, corner);
        CHECK(b.cell(2)->effective == 9); // only outlet height9; no exterior zero
    }
    void plateau() {
        for (Height height : {Height{0}, Height{-7}}) {
            const std::array<Height, 4> h{height, height, height, height};
            const std::array<Index, 1> out{3};
            const auto r = make(domain(2, 2), h, out);
            const std::array<Index, 4> parent{1, 3, 3, no_receiver}, rank{2, 1, 3, 0};
            for (Index i = 0; i < 4; ++i) {
                CHECK(r.cell(i)->parent == parent[static_cast<std::size_t>(i)] && r.cell(i)->rank == rank[static_cast<std::size_t>(i)]);
                CHECK(r.cell(i)->outlet == 3 && r.cell(i)->depth == 0);
            }
            const auto raw = analyze_drainage({domain(2, 2), h, out}, drainage_hard_limits).value();
            CHECK(raw.measurements().sinks == 3 && raw.measurements().outlet_contributing == 1);
            CHECK(r.measurements().filled_cells == 0 && r.measurements().volume == 0);
        }
        const std::array<Height, 4> varied{2, -4, 8, 1};
        const std::array<Index, 4> all{3, 0, 2, 1};
        const auto r = make(domain(2, 2), varied, all);
        CHECK(r.measurements().volume == 0 && r.measurements().outlets == 4);
        for (Index i = 0; i < 4; ++i) {
            CHECK(r.cell(i)->parent == no_receiver && r.cell(i)->outlet == i);
        }
    }
    void extremes() {
        const std::array<Index, 1> out{0};
        for (auto d : {domain(3, 1), domain(1, 3)}) {
            const std::array<Height, 3> h{high, low, low};
            const auto r = make(d, h, out);
            CHECK(r.cell(1)->depth == UINT64_C(4294967295) && r.cell(2)->depth == UINT64_C(4294967295));
            CHECK(r.measurements().volume == UINT64_C(8589934590) && r.measurements().filled_cells == 2);
        }
        for (Height height : {low, Height{-5}, high}) {
            const std::array<Height, 1> h{height};
            const auto r = make(domain(1, 1), h, out);
            CHECK(r.cell(0)->original == height && r.cell(0)->effective == height && r.cell(0)->rank == 0);
            CHECK(r.measurements().volume == 0 && r.usage().peak_queue == 1);
        }
        const std::array<Height, 3> negative{-5, -9, -8};
        const auto n = make(domain(1, 3), negative, out);
        CHECK((n.measurements() == SpillMeasurements{3, 1, 2, 4, 7, -5, -5}));
    }
    void exhaustive() {
        const auto d = domain(3, 2);
        const std::array<Index, 1> first{0}, last{5};
        const std::array<Index, 2> ends{0, 5};
        Index examined = 0;
        for (Index code = 0; code < 729; ++code) {
            std::array<Height, 6> h{};
            auto digits = code;
            for (auto& value : h) {
                value = static_cast<Height>(digits % 3) - 1;
                digits /= 3;
            }
            for (auto out : {std::span<const Index>{first}, std::span<const Index>{last}, std::span<const Index>{ends}}) {
                const auto r = make(d, h, out);
                CHECK(r.measurements().outlets == out.size());
                ++examined;
            }
        }
        CHECK(examined == 2187);
    }
    void generated() {
        for (Index seed : {0U, 1U, 42U, 991U}) {
            for (Index spacing : {3U, 6U}) {
                const auto field =
                    generate_elevation(
                        std::array<Index, 2>{17, 11}, {seed, 0x67656f6772617068, 7}, {spacing, -80, 120}, elevation_hard_limits
                    )
                        .value();
                const auto copy = field;
                const std::array<Index, 2> out{0, 186};
                const auto r = make(field.descriptor().domain, field.heights(), out);
                CHECK(r.measurements().cells == 187 && r.measurements().outlets == 2);
                CHECK(std::ranges::equal(field.heights(), copy.heights()) && field.descriptor() == copy.descriptor());
            }
        }
    }
    void invalid() {
        const auto d = domain(2, 2);
        const std::array<Height, 4> h{3, 2, 1, 0};
        const std::array<Index, 1> out{0};
        {
            Deny deny;
            error_is(analyze_spill({d, h, {}}, budget), FailureKind::invalid_input, SpillReason::outlets);
        }
        for (auto bad : {std::array<Index, 2>{0, 0}, {1, 4}, {0, no_receiver}}) {
            Deny deny;
            error_is(analyze_spill({d, h, bad}, budget), FailureKind::invalid_input, SpillReason::outlets);
        }
        const auto wrong = Domain::checked(std::array<Index, 3>{2, 2, 1}).value();
        {
            Deny deny;
            error_is(analyze_spill({wrong, h, out}, budget), FailureKind::invalid_input, SpillReason::dimensions);
        }
        {
            Deny deny;
            error_is(
                analyze_spill({d, std::span<const Height>(h).first(3), out}, budget), FailureKind::invalid_input, SpillReason::height_count
            );
        }
        {
            Deny deny;
            error_is(analyze_spill({domain(3, 1), h, out}, budget), FailureKind::invalid_input, SpillReason::height_count);
        }
        {
            Deny deny;
            error_is(spill_requirements(d, 5, 4), FailureKind::invalid_input, SpillReason::outlets);
        }
        {
            Deny deny;
            error_is(
                analyze_spill({d, h, out}, budget, {static_cast<SpillAlgorithm>(0), 1}), FailureKind::unsupported, SpillReason::algorithm
            );
        }
        for (auto version : {0U, 2U}) {
            Deny deny;
            error_is(
                analyze_spill({d, h, out}, budget, {SpillAlgorithm::priority_flood, version}), FailureKind::unsupported,
                SpillReason::version
            );
        }
        const auto huge = domain(std::numeric_limits<Index>::max(), 1);
        {
            Deny deny;
            error_is(spill_requirements(huge, 1, 1), FailureKind::invalid_input, SpillReason::overflow);
        }
        const auto volume = domain(std::numeric_limits<Index>::max() / UINT64_C(4294967295) + 1, 1);
        {
            Deny deny;
            error_is(spill_requirements(volume, 1, 1), FailureKind::invalid_input, SpillReason::overflow);
        }
        CHECK(!Domain::checked({}));
        CHECK(!Domain::checked(std::array<Index, 2>{0, 1}));
        CHECK(!Domain::checked(std::array<Index, 2>{std::numeric_limits<Index>::max(), 2}));
        const auto r = make(d, h, out);
        for (Index index : {Index{4}, no_receiver}) {
            Deny deny;
            error_is(r.cell(index), FailureKind::invalid_input, SpillReason::index);
        }
    }
    void limits() {
        const auto d = domain(2, 2);
        const std::array<Height, 4> h{0, 0, 0, 0};
        const std::array<Index, 1> out{3};
        const auto original = make(d, h, out), snapshot = original;
        const auto plan = spill_requirements(d, 1, 2).value();
        CHECK(plan.cells == 4 && plan.queue_capacity == 2 && plan.storage_bytes == 12464 && plan.minimum_work == 1074);
        auto exact = budget;
        exact.cells = 4;
        exact.storage_bytes = plan.storage_bytes;
        exact.queue_entries = 2;
        exact.queue_pushes = 4;
        const auto tight = analyze_spill({d, h, out}, exact).value();
        // Six heap extras: insert1 before2 swaps, two root replacements,
        // and insert0 before2 compares/swaps. Mandatory base is1074.
        CHECK(same(original, tight) && tight.usage().peak_queue == 2 && tight.usage().work == 1080);
        exact.work = tight.usage().work;
        CHECK(same(original, analyze_spill({d, h, out}, exact).value()));
        for (auto reason :
             {SpillReason::cells, SpillReason::storage, SpillReason::work, SpillReason::queue_pushes, SpillReason::queue_entries}) {
            auto lower = exact;
            if (reason == SpillReason::cells) {
                --lower.cells;
            }
            if (reason == SpillReason::storage) {
                --lower.storage_bytes;
            }
            if (reason == SpillReason::work) {
                lower.work = plan.minimum_work - 1;
            }
            if (reason == SpillReason::queue_pushes) {
                --lower.queue_pushes;
            }
            if (reason == SpillReason::queue_entries) {
                lower.queue_entries = 0;
            }
            Deny deny;
            error_is(analyze_spill({d, h, out}, lower), FailureKind::budget_refused, reason);
        }
        auto narrow = exact;
        narrow.queue_entries = 1;
        error_is(analyze_spill({d, h, out}, narrow), FailureKind::budget_refused, SpillReason::queue_entries);
        for (Index work = plan.minimum_work; work < tight.usage().work; ++work) {
            auto lower = exact;
            lower.work = work;
            error_is(analyze_spill({d, h, out}, lower), FailureKind::budget_refused, SpillReason::work);
            CHECK(same(original, snapshot) && h[0] == 0 && out[0] == 3);
        }
        const std::array<Height, 1> one{-7};
        const std::array<Index, 1> zero{0};
        const auto p = spill_requirements(domain(1, 1), 1, 1).value();
        const auto singleton = analyze_spill({domain(1, 1), one, zero}, {1, p.storage_bytes, p.minimum_work, 1, 1}).value();
        CHECK(singleton.usage().work == p.minimum_work && singleton.usage().heap_extra_work == 0);
        const auto too_large = domain(spill_hard_limits.cells + 1, 1);
        {
            Deny deny;
            error_is(analyze_spill({too_large, {}, zero}, budget), FailureKind::budget_refused, SpillReason::cells);
        }
        const auto maximum = domain(256, 256);
        const std::vector<Height> flat(static_cast<std::size_t>(maximum.cells()), -4);
        const auto accepted = analyze_spill({maximum, flat, zero}, budget).value();
        CHECK(accepted.measurements().cells == 65536 && accepted.measurements().filled_cells == 0);
        CHECK(accepted.usage().queue_pushes == 65536 && accepted.usage().queue_pops == 65536);
        CHECK(accepted.usage().work <= spill_requirements(maximum, 1, 65536)->maximum_work);
    }
    void ownership() {
        std::array<Height, 5> h{0, 5, -3, 5, 0};
        std::array<Index, 2> out{0, 4};
        const auto original = make(domain(5, 1), h, out), snapshot = original;
        CHECK(original.cells().data() != snapshot.cells().data());
        h.fill(7);
        out = {1, 3};
        CHECK(same(original, snapshot) && original.cell(2)->original == -3 && original.cell(0)->parent == no_receiver);
        bool propagated = false;
        try {
            Deny deny;
            static_cast<void>(analyze_spill({domain(5, 1), h, out}, budget));
        } catch (const std::bad_alloc&) {
            propagated = true;
        }
        CHECK(propagated && same(original, snapshot));
        auto refused = budget;
        refused.storage_bytes = 0;
        error_is(analyze_spill({domain(5, 1), h, out}, refused), FailureKind::budget_refused, SpillReason::storage);
        CHECK(same(original, snapshot));
        const auto lifetime = [] {
            const std::array<Height, 5> local{0, 5, -3, 5, 0};
            const std::array<Index, 2> outlets{0, 4};
            return analyze_spill({domain(5, 1), local, outlets}, budget).value();
        }();
        CHECK(same(lifetime, original));
        auto copy = original;
        copy = make(domain(5, 1), h, out);
        Spill moved = std::move(copy);
        CHECK(moved.cell(2)->original == 7 && same(original, snapshot));
        auto larger = budget;
        larger.work = std::numeric_limits<Index>::max();
        CHECK(
            same(original, analyze_spill({domain(5, 1), std::array<Height, 5>{0, 5, -3, 5, 0}, std::array<Index, 2>{4, 0}}, larger).value())
        );
    }
}
int main(int argc, char** argv) {
    struct Case {
        std::string_view name;
        void (*run)();
    };
    const std::array<Case, 9> cases{
        {{"spill_literal", literal},
         {"spill_saddles", saddles},
         {"spill_plateau", plateau},
         {"spill_extremes", extremes},
         {"spill_exhaustive", exhaustive},
         {"spill_generated", generated},
         {"spill_invalid", invalid},
         {"spill_limits", limits},
         {"spill_ownership", ownership}}
    };
    const std::string_view selection = argc == 2 ? argv[1] : "all";
    if (argc > 2) {
        std::cerr << "Expected one spill case name or all\n";
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
    std::cout << "Spill cases: " << passed << " passed, " << failed << " failed, 0 skipped; " << assertions << " assertions\n";
    return passed != 0 && failed == 0 ? 0 : 1;
}
