#include "cave.h"
#include "cave-checks.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>

// Separate executable: allocation denial has no effect on the retained maze
// runner. No actual large allocation or memory-pressure experiment is attempted.
namespace CaveAllocation {
    thread_local bool denied = false;
}
void* operator new(std::size_t size) {
    if (CaveAllocation::denied) {
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
    constexpr CaveBudgets budgets{4096, 128 * 1024, 2'000'000};
    constexpr StreamKey key{42, 0x63617665, 7};
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
        bool previous = CaveAllocation::denied;
        Deny() { CaveAllocation::denied = true; }
        ~Deny() { CaveAllocation::denied = previous; }
    };
    template <class T> void error_is(const Result<T>& result, FailureKind kind, Reason reason) {
        CHECK(!result);
        CHECK(result.error().kind == kind);
        CHECK(result.error().reason == reason);
    }
    bool same(const Cave& a, const Cave& b) {
        return a.domain() == b.domain() && a.effective() == b.effective() && a.measurements() == b.measurements() &&
               std::ranges::equal(a.occupancy(), b.occupancy());
    }
    void floors_are(const Cave& cave, std::initializer_list<Index> floor) {
        std::vector<std::uint8_t> expected(cave.occupancy().size(), 1);
        for (auto cell : floor) {
            expected.at(static_cast<std::size_t>(cell)) = 0;
        }
        CHECK(std::ranges::equal(cave.occupancy(), expected));
    }
    void verify(const Cave& cave) {
        const auto report = ProcGenCaveChecks::inspect(cave);
        const auto counts = cave.measurements();
        const auto needed = cave_requirements(cave.domain(), cave.effective()).value();
        CHECK(report.floor == counts.floor);
        CHECK(report.solid == counts.solid);
        CHECK(report.floor + report.solid == cave.domain().cells());
        CHECK(report.components.size() == counts.components);
        CHECK(counts.connected == (report.components.size() == 1));
        CHECK(counts.connectivity_required == (cave.effective().connectivity == CaveConnectivity::largest_floor));
        Index largest = 0;
        for (const auto& component : report.components) {
            largest = std::max(largest, static_cast<Index>(component.size()));
        }
        CHECK(largest == counts.largest_floor);
        CHECK(counts.generated_floor - counts.removed_floor == counts.floor);
        CHECK(counts.initial_interior_solid <= needed.interior_cells);
        CHECK(counts.generated_floor <= needed.interior_cells);
        CHECK(cave.effective() == cave.descriptor().requested);
        CHECK(cave.usage().work <= cave.descriptor().budgets.work);
        CHECK(cave.usage().random_draws >= needed.interior_cells);
        if (counts.connectivity_required) {
            CHECK(counts.components <= 1);
            CHECK(counts.connected == (counts.floor != 0));
        } else {
            CHECK(counts.removed_floor == 0);
            CHECK(counts.generated_components == counts.components);
        }
    }
    void initial() {
        const std::array<Index, 2> square{4, 4};
        const auto cave = generate_cave(square, {8, key.stage, key.stream}, {.solid_fill = 5000, .steps = 0}, budgets).value();
        // Hand-worked samples [7647,2549,2069,5797] in x-fast interior order.
        floors_are(cave, {5, 10});
        const auto measured = cave.measurements();
        CHECK(measured.initial_interior_solid == 2);
        CHECK(measured.generated_floor == 2);
        CHECK(measured.generated_components == 2);
        CHECK(cave.usage().random_draws == 4);
        verify(cave);
        const std::array<Index, 2> one{3, 3};
        const auto floor = generate_cave(one, key, {.solid_fill = 0, .steps = 0}, budgets).value();
        floors_are(floor, {4});
        CHECK(floor.measurements().initial_interior_solid == 0);
        CHECK(floor.usage().random_draws == 1);
        CHECK(floor.usage().work == 61);
        verify(floor);
        const auto solid = generate_cave(one, key, {.solid_fill = 10000, .steps = 0}, budgets).value();
        floors_are(solid, {});
        CHECK(solid.measurements().initial_interior_solid == 1);
        CHECK(solid.usage().random_draws == 1);
        CHECK(solid.usage().work == 55);
        verify(solid);
    }
    void smoothing() {
        const std::array<Index, 2> square{4, 4};
        const auto initial = generate_cave(square, {8, key.stage, key.stream}, {.solid_fill = 5000, .steps = 0}, budgets).value();
        const auto step =
            generate_cave(square, {8, key.stage, key.stream}, {.solid_fill = 5000, .steps = 1, .birth = 1 << 7, .survival = 0}, budgets)
                .value();
        floors_are(initial, {5, 10});
        floors_are(step, {6, 9});
        CHECK(step.measurements().initial_interior_solid == 2);
        verify(step);
        // An x-fast in-place evaluation would incorrectly leave cell10 floor.
        CHECK(step.occupancy()[10] == 1);
        const std::array<Index, 2> five{5, 5};
        const auto first = generate_cave(five, key, {.solid_fill = 0, .steps = 1}, budgets).value();
        floors_are(first, {7, 11, 12, 13, 17});
        verify(first);
        const auto second = generate_cave(five, key, {.solid_fill = 0, .steps = 2}, budgets).value();
        floors_are(second, {12});
        verify(second);
        const auto third = generate_cave(five, key, {.solid_fill = 0, .steps = 3}, budgets).value();
        floors_are(third, {});
        verify(third);
        const auto all_floor = generate_cave(five, key, {.solid_fill = 10000, .steps = 1, .birth = 0, .survival = 0}, budgets).value();
        floors_are(all_floor, {6, 7, 8, 11, 12, 13, 16, 17, 18});
        verify(all_floor);
        const auto all_solid = generate_cave(five, key, {.solid_fill = 0, .steps = 1, .birth = 0x1ff, .survival = 0x1ff}, budgets).value();
        floors_are(all_solid, {});
        verify(all_solid);
        // Neighbor-count endpoints 0 and8 are both meaningful and legal.
        const auto zero_count = generate_cave(five, key, {.solid_fill = 0, .steps = 1, .birth = 1, .survival = 0}, budgets).value();
        CHECK(zero_count.occupancy()[12] == 1);
        CHECK(zero_count.occupancy()[6] == 0);
        verify(zero_count);
    }
    void components() {
        const std::array<Index, 2> square{4, 4};
        const auto retained = generate_cave(square, {8, key.stage, key.stream}, {.solid_fill = 5000, .steps = 0}, budgets).value();
        const auto connected = generate_cave(
                                   square, {8, key.stage, key.stream},
                                   {.solid_fill = 5000, .steps = 0, .connectivity = CaveConnectivity::largest_floor}, budgets
        )
                                   .value();
        floors_are(retained, {5, 10});
        floors_are(connected, {5}); // equal size, root5 wins
        CHECK(!retained.measurements().connected);
        CHECK(!retained.measurements().connectivity_required);
        CHECK(connected.measurements().connectivity_required);
        CHECK(connected.measurements().connected);
        CHECK(connected.measurements().generated_components == 2);
        CHECK(connected.measurements().components == 1);
        CHECK(connected.measurements().removed_floor == 1);
        verify(retained);
        verify(connected);
        const std::array<Index, 2> line{6, 3};
        const auto unequal = generate_cave(line, key, {.solid_fill = 5000, .steps = 0}, budgets).value();
        const auto largest =
            generate_cave(line, key, {.solid_fill = 5000, .steps = 0, .connectivity = CaveConnectivity::largest_floor}, budgets).value();
        floors_are(unequal, {7, 9, 10});
        floors_are(largest, {9, 10});
        CHECK(largest.measurements().generated_floor == 3);
        CHECK(largest.measurements().largest_floor == 2);
        CHECK(largest.measurements().removed_floor == 1);
        verify(unequal);
        verify(largest);
    }
    void empty_tiny() {
        for (const auto& extents : {std::vector<Index>{1, 1}, {1, 7}, {8, 1}, {2, 6}, {5, 2}, {3, 3}}) {
            for (const auto policy : {CaveConnectivity::retain_components, CaveConnectivity::largest_floor}) {
                const auto cave = generate_cave(extents, key, {.solid_fill = 10000, .steps = 64, .connectivity = policy}, budgets).value();
                floors_are(cave, {});
                verify(cave);
                CHECK(cave.measurements().components == 0);
                CHECK(cave.measurements().removed_floor == 0);
                CHECK(cave.measurements().largest_floor == 0);
                CHECK(!cave.measurements().connected);
                CHECK(cave.measurements().connectivity_required == (policy == CaveConnectivity::largest_floor));
            }
        }
        const std::array<Index, 2> square{4, 4};
        const auto already =
            generate_cave(square, key, {.solid_fill = 0, .steps = 0, .connectivity = CaveConnectivity::largest_floor}, budgets).value();
        CHECK(already.measurements().floor == 4);
        CHECK(already.measurements().removed_floor == 0);
        verify(already);
    }
    void replay_cases() {
        const std::array<Index, 2> extents{17, 11};
        const std::array<CaveConfiguration, 2> configs{
            {{.solid_fill = 4200, .steps = 3, .birth = 0x1e0, .survival = 0x1f0},
             {.solid_fill = 5200, .steps = 4, .birth = 0x1c0, .survival = 0x1f0, .connectivity = CaveConnectivity::largest_floor}}
        };
        for (const auto seed : {0U, 1U, 42U, 991U}) {
            for (const auto config : configs) {
                const auto cave = generate_cave(extents, {seed, key.stage, key.stream}, config, budgets).value();
                verify(cave);
                CHECK(same(cave, replay_cave(cave.descriptor()).value()));
                auto descriptor = cave.descriptor();
                descriptor.budgets = cave_hard_limits;
                CHECK(same(cave, replay_cave(descriptor).value()));
                auto unrelated = IntegerStream::for_stage({seed, 0x6d617a65, key.stream});
                for (int i = 0; i < 100; ++i) {
                    static_cast<void>(unrelated.next());
                }
                CHECK(same(cave, replay_cave(cave.descriptor()).value()));
                auto retain = config;
                retain.connectivity = CaveConnectivity::retain_components;
                const auto before = generate_cave(extents, {seed, key.stage, key.stream}, retain, budgets).value();
                const auto report = ProcGenCaveChecks::inspect(before);
                CHECK(cave.measurements().generated_floor == report.floor);
                CHECK(cave.measurements().generated_components == report.components.size());
                if (config.connectivity == CaveConnectivity::largest_floor) {
                    std::vector<Index> best;
                    for (const auto& component : report.components) {
                        if (component.size() > best.size()) {
                            best = component;
                        }
                    }
                    std::vector<std::uint8_t> expected(cave.occupancy().size(), 1);
                    for (auto cell : best) {
                        expected[static_cast<std::size_t>(cell)] = 0;
                    }
                    CHECK(std::ranges::equal(cave.occupancy(), expected));
                }
            }
        }
    }
    void ownership() {
        const std::array<Index, 2> extents{7, 5};
        const auto original = generate_cave(extents, key, {.solid_fill = 0, .steps = 0}, budgets).value();
        const auto snapshot = original;
        auto copy = original;
        CHECK(copy.occupancy().data() != original.occupancy().data());
        copy = generate_cave(extents, key, {.solid_fill = 10000, .steps = 0}, budgets).value();
        CHECK(original.measurements().floor == 15);
        CHECK(copy.measurements().floor == 0);
        error_is(generate_cave(extents, key, {.solid_fill = 10001}, budgets), FailureKind::invalid_input, Reason::cave_fill);
        auto small = budgets;
        small.storage_bytes = 0;
        error_is(generate_cave(extents, key, {}, small), FailureKind::budget_refused, Reason::storage);
        CHECK(same(original, snapshot));
        CHECK(original.descriptor() == snapshot.descriptor());
        bool propagated = false;
        try {
            Deny deny;
            static_cast<void>(generate_cave(extents, key, {}, budgets));
        } catch (const std::bad_alloc&) {
            propagated = true;
        }
        CHECK(propagated);
        CHECK(same(original, snapshot));
        const auto lifetime = [] {
            const std::array<Index, 2> local{3, 3};
            return generate_cave(local, key, {.solid_fill = 0, .steps = 0}, budgets).value();
        }();
        floors_are(lifetime, {4});
        verify(lifetime);
        Cave moved = std::move(copy);
        verify(moved);
        verify(original);
    }
    void invalid() {
        const std::array<Index, 2> extents{4, 4};
        const auto domain = Domain::checked(extents).value();
        const std::array<CaveConfiguration, 5> configs{
            {{.solid_fill = 10001},
             {.steps = 65},
             {.birth = 0x200},
             {.survival = 0x8000},
             {.connectivity = static_cast<CaveConnectivity>(77)}}
        };
        const std::array<Reason, 5> reasons{
            Reason::cave_fill, Reason::cave_steps, Reason::cave_rules, Reason::cave_rules, Reason::cave_connectivity
        };
        for (std::size_t i = 0; i < configs.size(); ++i) {
            Deny deny;
            error_is(generate_cave(extents, key, configs[i], budgets), FailureKind::invalid_input, reasons[i]);
        }
        const std::array<Index, 1> one_axis{4};
        const std::array<Index, 3> three_axes{2, 2, 2};
        const std::array<Index, 2> zero{0, 2};
        {
            Deny deny;
            error_is(generate_cave({}, key, {}, budgets), FailureKind::invalid_input, Reason::cave_dimensions);
            error_is(generate_cave(one_axis, key, {}, budgets), FailureKind::invalid_input, Reason::cave_dimensions);
            error_is(generate_cave(three_axes, key, {}, budgets), FailureKind::invalid_input, Reason::cave_dimensions);
            error_is(generate_cave(zero, key, {}, budgets), FailureKind::invalid_input, Reason::extent);
        }
        CaveReplay descriptor{domain, key, {}, budgets};
        descriptor.version = 2;
        {
            Deny deny;
            error_is(replay_cave(descriptor), FailureKind::invalid_input, Reason::cave_version);
        }
        descriptor.version = 1;
        descriptor.algorithm = static_cast<CaveAlgorithm>(77);
        {
            Deny deny;
            error_is(replay_cave(descriptor), FailureKind::invalid_input, Reason::cave_algorithm);
        }
        descriptor.algorithm = CaveAlgorithm::cellular_2d;
        descriptor.domain = Domain::checked(three_axes).value();
        {
            Deny deny;
            error_is(replay_cave(descriptor), FailureKind::invalid_input, Reason::cave_dimensions);
        }
    }
    void limits() {
        const std::array<Index, 2> extents{3, 3};
        const auto domain = Domain::checked(extents).value();
        const CaveConfiguration config{.solid_fill = 0, .steps = 0};
        const auto needed = cave_requirements(domain, config).value();
        CHECK(needed.cells == 9);
        CHECK(needed.interior_cells == 1);
        CHECK(needed.storage_bytes == 4258);
        CHECK(needed.work_reservation == 61);
        const CaveBudgets exact{9, needed.storage_bytes, needed.work_reservation};
        const auto accepted = generate_cave(extents, key, config, exact).value();
        verify(accepted);
        CHECK(accepted.usage().work == 61);
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
            error_is(generate_cave(extents, key, config, lower), FailureKind::budget_refused, reason);
        }
        const std::array<Index, 2> product{maximum, 2}, bytes{maximum / 18 + 1, 1}, work{200000000, 200000000},
            over_cap{cave_hard_limits.cells + 1, 1};
        {
            Deny deny;
            error_is(generate_cave(product, key, config, budgets), FailureKind::invalid_input, Reason::overflow);
            error_is(generate_cave(bytes, key, config, budgets), FailureKind::invalid_input, Reason::overflow);
            error_is(generate_cave(work, key, {.steps = 64}, budgets), FailureKind::invalid_input, Reason::overflow);
            error_is(generate_cave(over_cap, key, config, cave_hard_limits), FailureKind::budget_refused, Reason::cells);
        }
        const auto solid = generate_cave(extents, key, {.solid_fill = 10000, .steps = 0}, exact).value();
        CHECK(solid.usage().work == 55); // admission intentionally reserves worst-case floor traversal
        auto conservative = exact;
        conservative.work = 55;
        {
            Deny deny;
            error_is(
                generate_cave(extents, key, {.solid_fill = 10000, .steps = 0}, conservative), FailureKind::budget_refused, Reason::work
            );
        }
    }
    void exhaustion() {
        const std::array<Index, 2> extents{3, 3};
        const CaveConfiguration config{.solid_fill = 0, .steps = 0};
        const StreamKey forced{0xd78884aaeab763e4, 0, 0}; // first word0 rejects below10000 (threshold1616)
        const auto complete = generate_cave(extents, forced, config, budgets).value();
        CHECK(complete.usage().random_draws == 2);
        CHECK(complete.usage().work == 62);
        verify(complete);
        const auto snapshot = complete;
        const auto needed = cave_requirements(complete.domain(), config).value();
        CaveBudgets exact{9, needed.storage_bytes, needed.work_reservation};
        error_is(generate_cave(extents, forced, config, exact), FailureKind::budget_refused, Reason::work);
        CHECK(same(complete, snapshot));
        CHECK(complete.descriptor() == snapshot.descriptor());
        ++exact.work;
        CHECK(same(complete, generate_cave(extents, forced, config, exact).value()));
    }
}
int main(int argc, char** argv) {
    struct Case {
        std::string_view name;
        void (*run)();
    };
    const std::array<Case, 9> cases{
        {{"cave_initial", initial},
         {"cave_smoothing", smoothing},
         {"cave_components", components},
         {"cave_empty", empty_tiny},
         {"cave_replay", replay_cases},
         {"cave_ownership", ownership},
         {"cave_invalid", invalid},
         {"cave_limits", limits},
         {"cave_exhaustion", exhaustion}}
    };
    const std::string_view selection = argc == 2 ? argv[1] : "all";
    if (argc > 2) {
        std::cerr << "Expected one cave case name or all\n";
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
    std::cout << "Cave cases: " << passed << " passed, " << failed << " failed, 0 skipped; " << assertions << " assertions\n";
    return failed == 0 && passed != 0 ? 0 : 1;
}
