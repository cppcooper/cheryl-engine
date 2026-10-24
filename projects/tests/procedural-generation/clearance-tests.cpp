#include "clearance.h"
#include "clearance-checks.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

namespace ClearanceAllocation {
    thread_local bool denied = false;
}
void* operator new(std::size_t size) {
    if (ClearanceAllocation::denied) {
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
    constexpr CaveBudgets budget{4096, 256 * 1024, 2'000'000};
    constexpr Index maximum = std::numeric_limits<Index>::max();
    std::size_t assertions = 0;
    void check(bool ok, const char* expression, int line) {
        ++assertions;
        if (!ok) {
            throw std::runtime_error("line " + std::to_string(line) + ": " + expression);
        }
    }
#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)
    struct Deny {
        bool previous = ClearanceAllocation::denied;
        Deny() { ClearanceAllocation::denied = true; }
        ~Deny() { ClearanceAllocation::denied = previous; }
    };
    template <class T> void error_is(const Result<T>& value, FailureKind kind, Reason reason) {
        CHECK(!value);
        CHECK(value.error().kind == kind);
        CHECK(value.error().reason == reason);
    }
    bool same(const Clearance& a, const Clearance& b) {
        return a.descriptor().domain == b.descriptor().domain && a.descriptor().minimum == b.descriptor().minimum &&
               a.descriptor().version == b.descriptor().version && a.measurements() == b.measurements() &&
               std::ranges::equal(a.distances(), b.distances()) && std::ranges::equal(a.usable(), b.usable());
    }
    void verify(OccupancyView input, const Clearance& field) {
        const auto oracle = ProcGenClearanceChecks::inspect(input, field);
        CHECK(oracle == field.measurements());
        CHECK(field.work() == 15 * input.domain.cells() + 6 * oracle.usable);
        CHECK(field.descriptor().version == 1);
        const auto needed = clearance_requirements(input.domain, static_cast<std::int64_t>(field.descriptor().minimum)).value();
        CHECK(field.work() >= needed.minimum_work);
        CHECK(field.work() <= needed.maximum_work);
    }
    void literal() {
        const auto small = Domain::checked(std::array<Index, 2>{3, 3}).value();
        const std::array<std::uint8_t, 9> floor{};
        const auto field = analyze_clearance({small, floor}, 2, budget).value();
        const std::array<Index, 9> expected{1, 1, 1, 1, 2, 1, 1, 1, 1};
        const std::array<std::uint8_t, 9> mask{0, 0, 0, 0, 1, 0, 0, 0, 0};
        CHECK(std::ranges::equal(field.distances(), expected));
        CHECK(std::ranges::equal(field.usable(), mask));
        CHECK((field.measurements() == ClearanceMeasurements{9, 0, 2, 1, 1, 1}));
        verify({small, floor}, field);
        const auto square = Domain::checked(std::array<Index, 2>{5, 5}).value();
        std::vector<std::uint8_t> mixed(25, 0);
        mixed[12] = 1;
        const auto original = mixed;
        const auto center = analyze_clearance({square, mixed}, 2, budget).value();
        const std::array<Index, 25> distances{1, 1, 1, 1, 1, 1, 2, 1, 2, 1, 1, 1, 0, 1, 1, 1, 2, 1, 2, 1, 1, 1, 1, 1, 1};
        CHECK(std::ranges::equal(center.distances(), distances));
        CHECK((center.measurements() == ClearanceMeasurements{24, 1, 2, 4, 4, 1}));
        CHECK(mixed == original);
        verify({square, mixed}, center);
        // One diagonal solid is two CARDINAL steps from center, while exterior
        // is three. A Moore/Chebyshev transform would incorrectly return one.
        mixed[12] = 0;
        mixed[6] = 1;
        const auto diagonal = analyze_clearance({square, mixed}, 2, budget).value();
        CHECK(diagonal.distances()[12] == 2);
        verify({square, mixed}, diagonal);
    }
    void outside() {
        for (const auto extents : {std::array<Index, 2>{1, 1}, {1, 7}, {8, 1}, {2, 6}, {5, 5}}) {
            const auto domain = Domain::checked(extents).value();
            std::vector<std::uint8_t> empty(static_cast<std::size_t>(domain.cells()), 0);
            const auto snapshot = empty;
            for (std::int64_t minimum : {0, 1, 2, 99}) {
                const auto field = analyze_clearance({domain, empty}, minimum, budget).value();
                verify({domain, empty}, field);
                CHECK(empty == snapshot);
                if (minimum <= 1) {
                    CHECK(field.measurements().usable == domain.cells());
                }
                if (extents[0] <= 2 || extents[1] <= 2) {
                    CHECK(field.measurements().maximum == 1);
                    CHECK(field.distances().front() == 1);
                    if (minimum > 1) {
                        CHECK(field.measurements().usable == 0);
                        CHECK(field.measurements().components == 0);
                    }
                }
            }
            std::fill(empty.begin(), empty.end(), std::uint8_t{1});
            const auto solid = analyze_clearance({domain, empty}, 0, budget).value();
            verify({domain, empty}, solid);
            CHECK((solid.measurements() == ClearanceMeasurements{0, domain.cells(), 0, 0, 0, 0}));
        }
    }
    void exhaustive() {
        const auto domain = Domain::checked(std::array<Index, 2>{3, 3}).value();
        std::array<std::uint8_t, 9> occupancy{};
        // All 512 tiny binary inputs, each checked at four thresholds against
        // exhaustive minima, not another wavefront implementation.
        for (unsigned bits = 0; bits < 512; ++bits) {
            for (unsigned cell = 0; cell < 9; ++cell) {
                occupancy[cell] = static_cast<std::uint8_t>((bits >> cell) & 1U);
            }
            for (std::int64_t minimum : {0, 1, 2, 3}) {
                const auto field = analyze_clearance({domain, occupancy}, minimum, budget).value();
                verify({domain, occupancy}, field);
                CHECK(field.measurements().floor + field.measurements().solid == 9);
            }
        }
    }
    void regions() {
        const auto domain = Domain::checked(std::array<Index, 2>{9, 5}).value();
        std::vector<std::uint8_t> occupancy(45, 1);
        for (Index y = 1; y <= 3; ++y) {
            for (Index x : {1U, 2U, 3U, 5U, 6U, 7U}) {
                occupancy[static_cast<std::size_t>(y * 9 + x)] = 0;
            }
        }
        occupancy[22] = 0; // one-cell connection between the two rooms
        const auto snapshot = occupancy;
        const auto all = analyze_clearance({domain, occupancy}, 1, budget).value();
        CHECK((all.measurements() == ClearanceMeasurements{19, 26, 2, 19, 1, 19}));
        verify({domain, occupancy}, all);
        const auto inset = analyze_clearance({domain, occupancy}, 2, budget).value();
        CHECK((inset.measurements() == ClearanceMeasurements{19, 26, 2, 4, 2, 2}));
        verify({domain, occupancy}, inset);
        CHECK(inset.usable()[20] == 1 && inset.usable()[21] == 1 && inset.usable()[23] == 1 && inset.usable()[24] == 1);
        CHECK(inset.usable()[22] == 0);
        CHECK(occupancy[22] == 0);
        CHECK(occupancy == snapshot);
        const auto too_high = analyze_clearance({domain, occupancy}, std::numeric_limits<std::int64_t>::max(), budget).value();
        CHECK(too_high.measurements().usable == 0);
        CHECK(too_high.measurements().maximum == 2);
        verify({domain, occupancy}, too_high);
        const auto tiny = Domain::checked(std::array<Index, 2>{3, 3}).value();
        const std::array<std::uint8_t, 9> diagonal{0, 1, 1, 1, 0, 1, 1, 1, 1};
        const auto islands = analyze_clearance({tiny, diagonal}, 1, budget).value();
        CHECK((islands.measurements() == ClearanceMeasurements{2, 7, 1, 2, 2, 1}));
        verify({tiny, diagonal}, islands);
    }
    void generated() {
        const std::array<Index, 2> extents{17, 11};
        for (const auto seed : {0U, 42U, 991U}) {
            for (const auto mode : {CaveConnectivity::retain_components, CaveConnectivity::largest_floor}) {
                const auto cave =
                    generate_cave(extents, {seed, 0x63617665, 7}, {.solid_fill = 4200, .steps = 3, .connectivity = mode}, budget).value();
                const auto descriptor = cave.descriptor();
                const std::vector<std::uint8_t> snapshot(cave.occupancy().begin(), cave.occupancy().end());
                for (std::int64_t minimum : {0, 2, 3}) {
                    const auto field = analyze_clearance({cave.domain(), cave.occupancy()}, minimum, budget).value();
                    verify({cave.domain(), cave.occupancy()}, field);
                    CHECK(field.measurements().floor == cave.measurements().floor);
                    CHECK(same(field, analyze_clearance({cave.domain(), cave.occupancy()}, minimum, clearance_hard_limits).value()));
                    CHECK(cave.descriptor() == descriptor);
                    CHECK(std::ranges::equal(cave.occupancy(), snapshot));
                }
            }
        }
    }
    void invalid() {
        const auto domain = Domain::checked(std::array<Index, 2>{3, 3}).value();
        std::array<std::uint8_t, 9> occupancy{};
        const auto other = Domain::checked(std::array<Index, 3>{3, 1, 3}).value();
        {
            Deny deny;
            error_is(analyze_clearance({domain, occupancy}, -1, budget), FailureKind::invalid_input, Reason::clearance_threshold);
            error_is(
                analyze_clearance({domain, occupancy}, std::numeric_limits<std::int64_t>::min(), budget), FailureKind::invalid_input,
                Reason::clearance_threshold
            );
            error_is(analyze_clearance({other, occupancy}, 0, budget), FailureKind::invalid_input, Reason::clearance_dimensions);
            error_is(analyze_clearance({domain, {}}, 0, budget), FailureKind::invalid_input, Reason::occupancy);
            error_is(analyze_clearance({domain, std::span(occupancy).first(8)}, 0, budget), FailureKind::invalid_input, Reason::occupancy);
        }
        for (std::uint8_t value : {std::uint8_t{2}, std::uint8_t{255}}) {
            occupancy[8] = value;
            Deny deny;
            error_is(analyze_clearance({domain, occupancy}, 0, budget), FailureKind::invalid_input, Reason::occupancy);
        }
        const std::array<std::uint8_t, 10> long_input{};
        {
            Deny deny;
            error_is(analyze_clearance({domain, long_input}, 0, budget), FailureKind::invalid_input, Reason::occupancy);
        }
        const auto byte_overflow = Domain::checked(std::array<Index, 2>{maximum / 17 + 1, 1}).value();
        const auto work_overflow = Domain::checked(std::array<Index, 2>{maximum / 21 + 1, 1}).value();
        {
            Deny deny;
            error_is(clearance_requirements(byte_overflow, 0), FailureKind::invalid_input, Reason::overflow);
            error_is(clearance_requirements(work_overflow, 0), FailureKind::invalid_input, Reason::overflow);
        }
    }
    void limits() {
        const auto domain = Domain::checked(std::array<Index, 2>{3, 3}).value();
        const std::array<std::uint8_t, 9> floor{};
        std::array<std::uint8_t, 9> solid{};
        solid.fill(1);
        const auto needed = clearance_requirements(domain, 0).value();
        CHECK(needed.cells == 9);
        CHECK(needed.storage_bytes == 4249);
        CHECK(needed.minimum_work == 135);
        CHECK(needed.maximum_work == 189);
        const CaveBudgets exact{9, 4249, 135};
        const auto empty = analyze_clearance({domain, solid}, 0, exact).value();
        CHECK(empty.work() == 135);
        verify({domain, solid}, empty);
        for (const auto reason : {Reason::cells, Reason::storage, Reason::work}) {
            auto lower = exact;
            if (reason == Reason::cells) {
                --lower.cells;
            }
            if (reason == Reason::storage) {
                --lower.storage_bytes;
            }
            if (reason == Reason::work) {
                --lower.work;
            }
            Deny deny;
            error_is(analyze_clearance({domain, solid}, 0, lower), FailureKind::budget_refused, reason);
        }
        auto all_budget = exact;
        all_budget.work = 189;
        const auto all = analyze_clearance({domain, floor}, 0, all_budget).value();
        CHECK(all.work() == 189);
        verify({domain, floor}, all);
        --all_budget.work;
        {
            Deny deny;
            error_is(analyze_clearance({domain, floor}, 0, all_budget), FailureKind::budget_refused, Reason::work);
        }
        auto center_budget = exact;
        center_budget.work = 141;
        const auto center = analyze_clearance({domain, floor}, 2, center_budget).value();
        CHECK(center.work() == 141);
        verify({domain, floor}, center);
        const auto snapshot = center;
        --center_budget.work; // admitted at135, then refuses during final traversal work
        error_is(analyze_clearance({domain, floor}, 2, center_budget), FailureKind::budget_refused, Reason::work);
        CHECK(same(center, snapshot));
    }
    void ownership() {
        const auto domain = Domain::checked(std::array<Index, 2>{3, 3}).value();
        std::vector<std::uint8_t> source(9, 0);
        const auto original = analyze_clearance({domain, source}, 2, budget).value();
        const auto snapshot = original;
        auto copy = original;
        CHECK(copy.distances().data() != original.distances().data());
        CHECK(copy.usable().data() != original.usable().data());
        source[4] = 1;
        copy = analyze_clearance({domain, source}, 2, budget).value();
        CHECK(original.distances()[4] == 2);
        CHECK(original.usable()[4] == 1);
        CHECK(copy.distances()[4] == 0);
        error_is(analyze_clearance({domain, source}, -1, budget), FailureKind::invalid_input, Reason::clearance_threshold);
        auto small = budget;
        small.storage_bytes = 0;
        error_is(analyze_clearance({domain, source}, 2, small), FailureKind::budget_refused, Reason::storage);
        bool propagated = false;
        try {
            Deny deny;
            static_cast<void>(analyze_clearance({domain, source}, 2, budget));
        } catch (const std::bad_alloc&) {
            propagated = true;
        }
        CHECK(propagated);
        CHECK(same(original, snapshot));
        const auto lifetime = [] {
            const auto local = Domain::checked(std::array<Index, 2>{1, 1}).value();
            const std::array<std::uint8_t, 1> input{0};
            return analyze_clearance({local, input}, 1, budget).value();
        }();
        CHECK(lifetime.distances()[0] == 1);
        CHECK(lifetime.usable()[0] == 1);
        Clearance moved = std::move(copy);
        verify({domain, source}, moved);
    }
}
int main(int argc, char** argv) {
    struct Case {
        std::string_view name;
        void (*run)();
    };
    const std::array<Case, 8> cases{
        {{"clearance_literal", literal},
         {"clearance_outside", outside},
         {"clearance_exhaustive", exhaustive},
         {"clearance_regions", regions},
         {"clearance_generated", generated},
         {"clearance_invalid", invalid},
         {"clearance_limits", limits},
         {"clearance_ownership", ownership}}
    };
    const std::string_view selection = argc == 2 ? argv[1] : "all";
    if (argc > 2) {
        std::cerr << "Expected one clearance case name or all\n";
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
    std::cout << "Clearance cases: " << passed << " passed, " << failed << " failed, 0 skipped; " << assertions << " assertions\n";
    return failed == 0 && passed != 0 ? 0 : 1;
}
