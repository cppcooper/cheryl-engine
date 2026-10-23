#include "maze.h"
#include "checks.h"

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

// Test-only allocation denial proves preflight refusals do not allocate and
// real allocation exceptions propagate. Inputs are prepared before denial;
// no large allocation or memory-pressure experiment is performed.
namespace AllocationProbe {
    thread_local bool denied = false;

    struct Deny {
        bool previous = denied;
        Deny() { denied = true; }
        ~Deny() { denied = previous; }
        Deny(const Deny&) = delete;
        Deny& operator=(const Deny&) = delete;
    };
}

void* operator new(std::size_t size) {
    if (AllocationProbe::denied) {
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
void operator delete(void* memory) noexcept {
    std::free(memory);
}
void operator delete[](void* memory) noexcept {
    std::free(memory);
}
void operator delete(void* memory, std::size_t) noexcept {
    std::free(memory);
}
void operator delete[](void* memory, std::size_t) noexcept {
    std::free(memory);
}

namespace {
    using namespace ProcGen;
    constexpr Budgets budgets{4096, 4095, 128 * 1024, 1'000'000};
    constexpr StreamKey key{42, 0x6d617a65, 7};
    constexpr auto maximum = std::numeric_limits<Index>::max();
    std::size_t assertions = 0;

    void check(bool passed, const char* expression, int line) {
        ++assertions;
        if (!passed) {
            throw std::runtime_error("line " + std::to_string(line) + ": " + expression);
        }
    }

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

    template <class T> void error_is(const Result<T>& result, FailureKind kind, Reason reason) {
        CHECK(!result);
        CHECK(result.error().kind == kind);
        CHECK(result.error().reason == reason);
    }

    template <class T> void invalid_is(const Result<T>& result, Reason reason) {
        error_is(result, FailureKind::invalid_input, reason);
    }

    template <class T> void refused_is(const Result<T>& result, Reason reason) {
        error_is(result, FailureKind::budget_refused, reason);
    }

    bool same_topology(const Maze& left, const Maze& right) {
        return left.domain() == right.domain() && std::ranges::equal(left.passages(), right.passages());
    }

    void rng() {
        IntegerStream stream(0);
        constexpr std::array<std::uint64_t, 6> vector{0xe220a8397b1dcdaf, 0x6e789e6aa1b965f4, 0x06c45d188009454f,
                                                      0xf88bb8a8724c81ec, 0x1b39896a51a8749b, 0x53cb9f0c747ea2ea};
        for (const auto expected : vector) {
            CHECK(stream.next() == expected);
        }
        CHECK(derive_stream_state({0, 0, 0}) == UINT64_C(0x01fc6447c3b77a8b));
        CHECK(derive_stream_state(key) == UINT64_C(0x8516db164d0a8eeb));
        CHECK(derive_stream_state({42, 0x63617665, 7}) == UINT64_C(0xfc7895c243a9393e));
        CHECK(derive_stream_state({maximum, maximum, maximum}) == UINT64_C(0xcaaba710d176152a));
        auto stage = IntegerStream::for_stage(key);
        constexpr std::array<std::uint64_t, 4> stage_vector{0x9276deb25e0e27e0, 0x2ec06c357d1cf02c, 0x0583fa4b80b950a4, 0xfda286cbfd8190a5};
        for (const auto expected : stage_vector) {
            CHECK(stage.next() == expected);
        }
        // First raw draw for state 3 is below 2^63-1: reject it; second accepts.
        IntegerStream rejection(3);
        Work work(2);
        const auto sampled = sample_below(rejection, (UINT64_C(1) << 63) + 1, work);
        CHECK(sampled.value() == UINT64_C(0x33466f8a7b81a988));
        CHECK(work.used() == 2);
        CHECK(rejection.next() == UINT64_C(0x9cebe8a6d050dd01));
        IntegerStream bounded(0);
        Work bounded_work(128);
        for (const auto bound : std::array<Index, 5>{1, 2, 3, 17, maximum}) {
            for (int repeat = 0; repeat < 8; ++repeat) {
                CHECK(sample_below(bounded, bound, bounded_work).value() < bound);
            }
        }
        Work unit_work(1);
        IntegerStream unit(0);
        CHECK(sample_below(unit, 1, unit_work).value() == 0);
        CHECK(unit_work.used() == 1);
        CHECK(unit.next() == vector[1]);
        Work count(maximum);
        CHECK(count.consume(maximum));
        CHECK(!count.consume(1));
        CHECK(count.used() == maximum);
        CHECK(count.consume(0));
    }

    void domain() {
        const std::array<Index, 4> extents{3, 2, 4, 2};
        const auto domain = Domain::checked(extents).value();
        const std::array<Index, 4> expected_strides{1, 3, 6, 24};
        CHECK(std::ranges::equal(domain.strides(), expected_strides));
        CHECK(domain.cells() == 48);
        for (Index index = 0; index < domain.cells(); ++index) {
            const auto coordinate = domain.coordinates(index).value();
            CHECK(domain.index({coordinate.data(), extents.size()}).value() == index);
            for (std::size_t axis = 0; axis < extents.size(); ++axis) {
                for (const auto direction : {Direction::negative, Direction::positive}) {
                    const auto next = domain.neighbor(index, axis, direction).value();
                    if (next) {
                        const auto moved = domain.coordinates(*next).value();
                        for (std::size_t other = 0; other < extents.size(); ++other) {
                            if (other != axis) {
                                CHECK(coordinate[other] == moved[other]);
                            } else if (direction == Direction::negative) {
                                CHECK(moved[axis] + 1 == coordinate[axis]);
                            } else {
                                CHECK(moved[axis] == coordinate[axis] + 1);
                            }
                        }
                    } else {
                        CHECK(direction == Direction::negative ? coordinate[axis] == 0 : coordinate[axis] == extents[axis] - 1);
                    }
                }
            }
        }
    }

    void topology() {
        const std::array<std::vector<Index>, 7> domains{std::vector<Index>{1}, {1, 1, 1, 1}, {1, 9}, {7}, {1, 5, 1, 2}, {2, 3, 4},
                                                        {2, 1, 2, 1, 3, 1}};
        for (const auto& extents : domains) {
            for (const auto seed : std::array<Index, 3>{0, 9, maximum}) {
                const auto maze = ProcGen::generate(extents, {seed, key.stage, key.stream}, budgets).value();
                const auto report = ProcGenChecks::verify(maze);
                CHECK(report.cells == maze.domain().cells());
                CHECK(report.edges + 1 == report.cells);
                CHECK(maze.usage().work <= budgets.work);
                if (report.cells == 1) {
                    CHECK(maze.passages()[0] == 0);
                    CHECK(maze.usage().random_draws == 0);
                }
            }
        }
        std::array<Index, max_dimensions> extents{};
        extents.fill(1);
        extents.back() = 2;
        const auto high_axis = generate(extents, key, budgets).value();
        const auto report = ProcGenChecks::verify(high_axis);
        CHECK(report.axis_edges[15] == 1);
        CHECK(high_axis.passages()[0] == (UINT32_C(1) << 31));
        CHECK(high_axis.passages()[1] == (UINT32_C(1) << 30));
    }

    void four_d() {
        const std::array<Index, 4> extents{2, 3, 2, 4};
        for (const auto seed : std::array<Index, 6>{0, 1, 2, 19, 42, maximum}) {
            const auto maze = ProcGen::generate(extents, {seed, key.stage, key.stream}, budgets).value();
            const auto report = ProcGenChecks::verify(maze);
            CHECK(report.cells == 48);
            CHECK(report.reached == 48);
            CHECK(report.edges == 47);
            CHECK(report.axis_edges[3] >= 3);
            const auto repeated = ProcGen::generate(extents, {seed, key.stage, key.stream}, budgets).value();
            CHECK(same_topology(maze, repeated));
        }
    }

    void replay_case() {
        const std::array<Index, 2> extents{2, 2};
        const auto maze = generate(extents, key, budgets).value();
        // Hand-traced v1 edges: 0--1, 1--3, 3--2. Axis 0 fastest.
        constexpr std::array<std::uint32_t, 4> fixture{2, 9, 2, 5};
        CHECK(std::ranges::equal(maze.passages(), fixture));
        CHECK(maze.usage().random_draws == 3);
        CHECK(maze.usage().work == 46);
        CHECK(maze.descriptor().random == key);
        CHECK(maze.descriptor().budgets == budgets);
        CHECK(maze.descriptor().algorithm == MazeAlgorithm::depth_first);
        CHECK(maze.descriptor().version == 1);
        CHECK(std::ranges::equal(maze.descriptor().domain.extents(), extents));
        const auto reproduced = ProcGen::replay(maze.descriptor()).value();
        CHECK(same_topology(maze, reproduced));
        CHECK(maze.descriptor() == reproduced.descriptor());
        auto larger = maze.descriptor();
        larger.budgets = hard_limits;
        const auto admitted = ProcGen::replay(larger).value();
        CHECK(same_topology(maze, admitted));
        CHECK(admitted.usage().work == maze.usage().work);
        CHECK(admitted.descriptor().budgets == hard_limits);
        auto unrelated = IntegerStream::for_stage({42, 0x63617665, 7});
        for (int draw = 0; draw < 1000; ++draw) {
            static_cast<void>(unrelated.next());
        }
        const auto after_unrelated = generate(extents, key, budgets).value();
        CHECK(same_topology(maze, after_unrelated));
        auto original_stage = IntegerStream::for_stage(key);
        CHECK(original_stage.next() == UINT64_C(0x9276deb25e0e27e0));
    }

    void ownership() {
        const std::array<Index, 2> extents{3, 4};
        const auto original = generate(extents, key, budgets).value();
        const auto snapshot = original;
        CHECK(original.passages().data() != snapshot.passages().data());
        const auto other = generate(extents, {123, key.stage, key.stream}, budgets).value();
        CHECK(original.passages().data() != other.passages().data());
        CHECK(same_topology(original, snapshot));
        auto copied = [&] {
            const auto temporary = generate(extents, key, budgets).value();
            return Maze(temporary);
        }();
        CHECK(same_topology(original, copied));
        const auto moved = std::move(copied);
        CHECK(same_topology(original, moved));
        auto refused = budgets;
        refused.cells = 1;
        refused_is(generate(extents, key, refused), Reason::cells);
        const std::array<Index, 2> invalid_extents{3, 0};
        invalid_is(generate(invalid_extents, key, budgets), Reason::extent);
        CHECK(same_topology(original, snapshot));
        CHECK(original.descriptor() == snapshot.descriptor());
        bool propagated = false;
        try {
            const AllocationProbe::Deny deny;
            static_cast<void>(generate(extents, key, budgets));
        } catch (const std::bad_alloc&) {
            propagated = true;
        }
        CHECK(propagated);
        CHECK(same_topology(original, snapshot));
        CHECK(ProcGenChecks::verify(original).reached == original.domain().cells());
    }

    void invalid() {
        const std::array<Index, 2> extents{2, 3};
        const auto valid_domain = Domain::checked(extents).value();
        const auto maze = generate(extents, key, budgets).value();
        const std::array<Index, 2> zeros{2, 0};
        const std::array<Index, 2> bad_coordinate{2, 0};
        const std::array<Index, 1> short_coordinate{0};
        std::array<Index, max_dimensions + 1> too_many{};
        too_many.fill(1);
        {
            const AllocationProbe::Deny deny;
            invalid_is(generate({}, key, budgets), Reason::dimensions);
            invalid_is(generate(too_many, key, budgets), Reason::dimensions);
            invalid_is(generate(zeros, key, budgets), Reason::extent);
            invalid_is(valid_domain.index(bad_coordinate), Reason::coordinate);
            invalid_is(valid_domain.index(short_coordinate), Reason::coordinate);
            invalid_is(valid_domain.coordinates(valid_domain.cells()), Reason::index);
            invalid_is(valid_domain.coordinates(maximum), Reason::index);
            invalid_is(valid_domain.neighbor(valid_domain.cells(), 0, Direction::positive), Reason::index);
            invalid_is(valid_domain.neighbor(0, 2, Direction::positive), Reason::axis);
            invalid_is(valid_domain.neighbor(0, 0, static_cast<Direction>(2)), Reason::direction);
            invalid_is(maze.neighbor(maze.domain().cells(), 0, Direction::positive), Reason::index);
            invalid_is(maze.neighbor(0, 2, Direction::positive), Reason::axis);
            invalid_is(maze.neighbor(0, 0, static_cast<Direction>(std::numeric_limits<std::uint32_t>::max())), Reason::direction);
        }
        auto descriptor = maze.descriptor();
        for (const auto version : {0U, 2U, std::numeric_limits<std::uint32_t>::max()}) {
            descriptor.version = version;
            const AllocationProbe::Deny deny;
            invalid_is(ProcGen::replay(descriptor), Reason::version);
        }
        descriptor.version = 1;
        descriptor.algorithm = static_cast<MazeAlgorithm>(0);
        {
            const AllocationProbe::Deny deny;
            invalid_is(ProcGen::replay(descriptor), Reason::algorithm);
        }
        Work work(1);
        IntegerStream stream(0);
        invalid_is(sample_below(stream, 0, work), Reason::bound);
        CHECK(work.used() == 0);
        CHECK(stream.next() == UINT64_C(0xe220a8397b1dcdaf));
    }

    void overflow() {
        const std::array<Index, 2> product_overflow{maximum, 2};
        const std::array<Index, 2> index_overflow{UINT64_C(1) << 63, 2};
        const std::array<Index, 1> bytes_overflow{maximum / 12 + 1};
        const std::array<Index, 1> fixed_overflow{maximum / 12};
        const std::array<Index, 1> maximal{maximum};
        const auto largest_domain = Domain::checked(maximal).value();
        CHECK(largest_domain.cells() == maximum);
        CHECK(largest_domain.coordinates(maximum - 1).value()[0] == maximum - 1);
        const std::array<Index, 1> last{maximum - 1};
        CHECK(largest_domain.index(last).value() == maximum - 1);
        CHECK(!largest_domain.neighbor(maximum - 1, 0, Direction::positive).value());
        CHECK(largest_domain.neighbor(maximum - 2, 0, Direction::positive).value() == maximum - 1);
        std::array<Index, max_dimensions> work_overflow{};
        work_overflow.fill(1);
        work_overflow[0] = maximum / 64;
        const std::array<Index, 1> container_overflow{static_cast<Index>(std::vector<Index>{}.max_size()) + 1};
        const AllocationProbe::Deny deny;
        invalid_is(generate(product_overflow, key, budgets), Reason::overflow);
        invalid_is(generate(index_overflow, key, budgets), Reason::overflow);
        invalid_is(generate(bytes_overflow, key, budgets), Reason::overflow);
        invalid_is(generate(fixed_overflow, key, budgets), Reason::overflow);
        invalid_is(generate(maximal, key, budgets), Reason::overflow);
        invalid_is(generate(work_overflow, key, budgets), Reason::overflow);
        invalid_is(generate(container_overflow, key, budgets), Reason::overflow);
    }

    void budget_cases() {
        const std::array<Index, 2> extents{2, 3};
        const auto valid_domain = Domain::checked(extents).value();
        const auto needed = requirements(valid_domain).value();
        CHECK(needed.cells == 6);
        CHECK(needed.edges == 5);
        CHECK(needed.storage_bytes == 4168);
        CHECK(needed.minimum_work == 72);
        const Budgets exact{needed.cells, needed.edges, needed.storage_bytes, needed.minimum_work};
        const auto admitted = generate(extents, key, exact).value();
        CHECK(ProcGenChecks::verify(admitted).reached == 6);
        CHECK(admitted.usage().work == needed.minimum_work);
        CHECK(admitted.usage().random_draws == needed.edges);
        auto limit = exact;
        --limit.cells;
        {
            const AllocationProbe::Deny deny;
            refused_is(generate(extents, key, limit), Reason::cells);
        }
        limit = exact;
        --limit.edges;
        {
            const AllocationProbe::Deny deny;
            refused_is(generate(extents, key, limit), Reason::edges);
        }
        limit = exact;
        --limit.storage_bytes;
        {
            const AllocationProbe::Deny deny;
            refused_is(generate(extents, key, limit), Reason::storage);
        }
        limit = exact;
        --limit.work;
        {
            const AllocationProbe::Deny deny;
            refused_is(generate(extents, key, limit), Reason::work);
            refused_is(generate(extents, key, {0, 0, 0, 0}), Reason::cells);
        }
        const std::array<Index, 1> one{1};
        const auto single = generate(one, key, {1, 0, 4108, 5}).value();
        CHECK(ProcGenChecks::verify(single).edges == 0);
        const std::array<Index, 1> too_large{hard_limits.cells + 1};
        const Budgets unlimited{maximum, maximum, maximum, maximum};
        {
            const AllocationProbe::Deny deny;
            refused_is(generate(too_large, key, unlimited), Reason::cells);
            refused_is(generate(one, key, {1, 0, 4107, 5}), Reason::storage);
            refused_is(generate(one, key, {1, 0, 4108, 4}), Reason::work);
        }
        CHECK(same_topology(admitted, generate(extents, key, budgets).value()));
    }

    void exhaustion() {
        IntegerStream stream(3);
        Work one_draw(1);
        refused_is(sample_below(stream, (UINT64_C(1) << 63) + 1, one_draw), Reason::work);
        CHECK(one_draw.used() == 1);
        CHECK(stream.next() == UINT64_C(0xb3466f8a7b81a989));
        Work zero_draws(0);
        refused_is(sample_below(stream, 1, zero_draws), Reason::work);
        CHECK(zero_draws.used() == 0);

        // Derived algebraically by inverting the stream finalizer, not by
        // unbounded seed search. First draw is 0; root of a 2x2x2 domain has
        // three choices, whose threshold is 1. Mandatory work admits the call,
        // but this rejected draw forces exhaustion during actual traversal.
        const StreamKey rejecting_key{0xd78884aaeab763e4, 0, 0};
        auto forced = IntegerStream::for_stage(rejecting_key);
        CHECK(forced.next() == 0);
        const std::array<Index, 3> extents{2, 2, 2};
        const auto valid_domain = Domain::checked(extents).value();
        const auto needed = requirements(valid_domain).value();
        const Budgets minimum{needed.cells, needed.edges, needed.storage_bytes, needed.minimum_work};
        const auto previous = generate(extents, key, budgets).value();
        const auto snapshot = previous;
        refused_is(generate(extents, rejecting_key, minimum), Reason::work);
        CHECK(same_topology(previous, snapshot));
        CHECK(previous.descriptor() == snapshot.descriptor());
        auto sufficient = minimum;
        sufficient.work += 32;
        const auto accepted = generate(extents, rejecting_key, sufficient).value();
        CHECK(accepted.usage().random_draws > needed.edges);
        CHECK(accepted.usage().work == needed.minimum_work + accepted.usage().random_draws - needed.edges);
        CHECK(ProcGenChecks::verify(accepted).reached == 8);
        auto exhausted_replay = accepted.descriptor();
        exhausted_replay.budgets = minimum;
        refused_is(ProcGen::replay(exhausted_replay), Reason::work);
        CHECK(same_topology(previous, snapshot));
    }
}

int main(int argc, char** argv) {
    struct Case {
        std::string_view name;
        void (*run)();
    };
    const std::array<Case, 10> cases{
        {{"rng", rng},
         {"domain", domain},
         {"topology", topology},
         {"four_d", four_d},
         {"replay", replay_case},
         {"ownership", ownership},
         {"invalid", invalid},
         {"overflow", overflow},
         {"budgets", budget_cases},
         {"exhaustion", exhaustion}}
    };
    const std::string_view selection = argc == 2 ? argv[1] : "all";
    if (argc > 2) {
        std::cerr << "Expected one case name or all\n";
        return 2;
    }
    std::size_t passed = 0;
    std::size_t failed = 0;
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
    std::cout << "Cases: " << passed << " passed, " << failed << " failed, 0 skipped; " << assertions << " assertions\n";
    return failed == 0 && passed != 0 ? 0 : 1;
}
