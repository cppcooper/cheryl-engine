#include "configuration.h"
#include "checks.h"
#include "traversal-tests.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <new>
#include <ranges>
#include <stdexcept>
#include <string>

namespace AllocationProbe {
    extern thread_local bool denied;
}

namespace ProcGenTests {
    namespace {
        using namespace ProcGen;
        constexpr Budgets budgets{4096, 4095, 128 * 1024, 1'000'000};
        constexpr SearchLimits limits{{4096, 65536, 2 * 1024 * 1024, 2'000'000}, 4096, 8192, 16384};
        constexpr StreamKey key{42, 0x6d617a65, 7};
        constexpr Index maximum = std::numeric_limits<Index>::max();
        std::size_t assertions = 0;
        void check(bool ok, const char* text, int line) {
            ++assertions;
            if (!ok) {
                throw std::runtime_error("line " + std::to_string(line) + ": " + text);
            }
        }
#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)
        struct Deny {
            bool before = AllocationProbe::denied;
            Deny() { AllocationProbe::denied = true; }
            ~Deny() { AllocationProbe::denied = before; }
        };
        template <class T> void error_is(const Result<T>& result, FailureKind kind, Reason reason) {
            CHECK(!result);
            CHECK(result.error().kind == kind);
            CHECK(result.error().reason == reason);
        }
        bool same(const ConfiguredMaze& left, const ConfiguredMaze& right) {
            return left.domain() == right.domain() && left.endpoints() == right.endpoints() && left.effective() == right.effective() &&
                   std::ranges::equal(left.passages(), right.passages()) && std::ranges::equal(left.main_route(), right.main_route());
        }
        Coordinate decode(Index cell, std::span<const Index> extents) {
            Coordinate coordinate{};
            for (std::size_t axis = 0; axis < extents.size(); ++axis) {
                coordinate[axis] = cell % extents[axis];
                cell /= extents[axis];
            }
            return coordinate;
        }

        // Independent pairwise-coordinate edge extraction, then plain FIFO BFS.
        // No construction order, Domain::neighbor, generator or A* helpers.
        struct Oracle {
            std::vector<std::vector<Index>> adjacency;
            explicit Oracle(const ConfiguredMaze& maze)
            : adjacency(static_cast<std::size_t>(maze.domain().cells())) {
                CHECK(adjacency.size() <= 64);
                for (Index a = 0; a < adjacency.size(); ++a) {
                    for (Index b = a + 1; b < adjacency.size(); ++b) {
                        const auto ca = decode(a, maze.domain().extents());
                        const auto cb = decode(b, maze.domain().extents());
                        std::size_t differences = 0, changed = 0;
                        for (std::size_t axis = 0; axis < maze.domain().dimensions(); ++axis) {
                            if (ca[axis] != cb[axis]) {
                                ++differences;
                                changed = axis;
                            }
                        }
                        if (differences == 1 && cb[changed] == ca[changed] + 1 &&
                            (maze.passages()[static_cast<std::size_t>(a)] & (std::uint32_t{1} << (2 * changed + 1))) != 0) {
                            adjacency[static_cast<std::size_t>(a)].push_back(b);
                            adjacency[static_cast<std::size_t>(b)].push_back(a);
                        }
                    }
                }
            }
            Index distance(Index first, Index last) const {
                std::vector<Index> distances(adjacency.size(), maximum);
                std::vector<Index> queue{first};
                distances[static_cast<std::size_t>(first)] = 0;
                for (std::size_t head = 0; head < queue.size(); ++head) {
                    const auto current = queue[head];
                    for (const auto next : adjacency[static_cast<std::size_t>(current)]) {
                        if (distances[static_cast<std::size_t>(next)] == maximum) {
                            distances[static_cast<std::size_t>(next)] = distances[static_cast<std::size_t>(current)] + 1;
                            queue.push_back(next);
                        }
                    }
                }
                CHECK(queue.size() == adjacency.size());
                return distances[static_cast<std::size_t>(last)];
            }
            void path(std::span<const Index> route, Endpoints endpoints, Index cost) const {
                CHECK(!route.empty());
                CHECK(route.front() == endpoints.entrance);
                CHECK(route.back() == endpoints.goal);
                CHECK(cost == route.size() - 1);
                std::vector<bool> seen(adjacency.size(), false);
                for (std::size_t i = 0; i < route.size(); ++i) {
                    CHECK(route[i] < adjacency.size());
                    CHECK(!seen[static_cast<std::size_t>(route[i])]);
                    seen[static_cast<std::size_t>(route[i])] = true;
                    if (i > 0) {
                        CHECK(
                            std::ranges::find(adjacency[static_cast<std::size_t>(route[i - 1])], route[i]) !=
                            adjacency[static_cast<std::size_t>(route[i - 1])].end()
                        );
                    }
                }
            }
        };

        void verify(const ConfiguredMaze& maze) {
            const auto topology = ProcGenChecks::verify(maze_view(maze));
            const auto cells = maze.domain().cells();
            const auto length = maze.effective().route_cells;
            const auto endpoints = maze.endpoints();
            CHECK(topology.cells == cells);
            CHECK(topology.edges == cells - 1);
            CHECK(topology.reached == cells);
            CHECK(maze.main_route().size() == length);
            const Oracle oracle(maze);
            const auto distance = oracle.distance(endpoints.entrance, endpoints.goal);
            CHECK(distance == length - 1);
            oracle.path(maze.main_route(), endpoints, distance);
            for (const auto heuristic : {Heuristic::zero, Heuristic::manhattan}) {
                const auto search = find_route(maze_view(maze), endpoints.entrance, endpoints.goal, heuristic, limits).value();
                CHECK(search.path.has_value());
                CHECK(search.path->cost == distance);
                CHECK(std::ranges::equal(search.path->cells, maze.main_route()));
                oracle.path(search.path->cells, endpoints, search.path->cost);
            }
            const auto& measured = maze.measurements();
            CHECK(measured.active_cells == cells);
            CHECK(measured.edges == cells - 1);
            CHECK(measured.reachable == cells);
            CHECK(measured.components == 1);
            CHECK(measured.cycle_rank == 0);
            CHECK(measured.route->cells == length);
            CHECK(measured.route->cost == length - 1);
            CHECK(measured.tree_branches->off_route_cells == cells - length);
            CHECK(measured.tree_branches->off_route_edges == cells - length);
            CHECK(measured.strict_labyrinth == (length == cells));
            if (length == cells && cells > 1) {
                CHECK(measured.degrees[1] == 2);
                CHECK(measured.degrees[2] == cells - 2);
                CHECK(std::popcount(maze.passages()[static_cast<std::size_t>(endpoints.entrance)]) == 1);
                CHECK(std::popcount(maze.passages()[static_cast<std::size_t>(endpoints.goal)]) == 1);
                CHECK((measured.tree_branches == TreeBranches{0, 0, 0, 0, 0}));
            }
            const auto needed = configured_requirements(maze.domain(), maze.descriptor().requested).value();
            CHECK(needed.resources.minimum_work <= maze.usage().work);
            CHECK(maze.usage().work <= budgets.work);
        }

        void profile() {
            const std::array<Index, 2> extents{5, 3};
            for (const auto seed : {0U, 42U, 991U}) {
                Index previous = 0;
                for (const auto value : {0.0, 0.25, 0.5, 0.75, 1.0}) {
                    const auto maze =
                        generate_configured(extents, {seed, key.stage, key.stream}, {.traversability = value}, budgets).value();
                    const Index literal_length = 2 + static_cast<Index>(value * 13); // inputs exactly representable, C=15
                    CHECK(maze.main_route().size() == literal_length);
                    CHECK(maze.main_route().size() >= previous);
                    CHECK(maze.descriptor().requested.traversability == value);
                    previous = maze.main_route().size();
                    verify(maze);
                }
            }
            // Pinned, hand-traced 2x2: shuffled fast axis Y, no reflections.
            // Prefix 0,2,3 and the remaining cell 1 attached to 3 (draw chooses 1).
            const std::array<Index, 2> square{2, 2};
            const auto middle = generate_configured(square, key, {.traversability = .5}, budgets).value();
            const std::array<Index, 3> route{0, 2, 3};
            const std::array<std::uint32_t, 4> masks{8, 8, 6, 5};
            CHECK(std::ranges::equal(middle.main_route(), route));
            CHECK(std::ranges::equal(middle.passages(), masks));
            CHECK((middle.measurements().tree_branches == TreeBranches{1, 1, 1, 1, 1}));
            std::cout << "2x2 middle work=" << middle.usage().work << "; draws=" << middle.usage().random_draws << '\n';
            CHECK(middle.usage().work == 147);
            CHECK(middle.usage().random_draws == 4);
            CHECK(!middle.measurements().strict_labyrinth); // tree's unique selected route is insufficient
            const auto strict = generate_configured(square, key, {.traversability = 1}, budgets).value();
            const std::array<Index, 4> full{0, 2, 3, 1};
            CHECK(std::ranges::equal(strict.main_route(), full));
            CHECK(std::ranges::equal(strict.passages(), masks));
            std::cout << "2x2 strict work=" << strict.usage().work << "; draws=" << strict.usage().random_draws << '\n';
            CHECK(strict.usage().work == 150);
            CHECK(strict.usage().random_draws == 3);
            verify(strict);
        }
        void four_d() {
            const std::array<Index, 4> extents{3, 2, 2, 3};
            for (const auto seed : {0U, 1U, 42U, 991U}) {
                for (const auto value : {0.0, .5, 1.0}) {
                    const auto maze =
                        generate_configured(extents, {seed, key.stage, key.stream}, {.traversability = value}, budgets).value();
                    verify(maze);
                    CHECK(ProcGenChecks::verify(maze_view(maze)).axis_edges[3] > 0);
                    if (value >= .5) {
                        Index fourth_steps = 0;
                        for (std::size_t i = 1; i < maze.main_route().size(); ++i) {
                            const auto a = decode(maze.main_route()[i - 1], extents);
                            const auto b = decode(maze.main_route()[i], extents);
                            if (a[3] != b[3]) {
                                ++fourth_steps;
                            }
                        }
                        CHECK(fourth_steps > 0);
                    }
                }
            }
        }
        void tiny() {
            for (const auto& extents : {std::vector<Index>{1}, {1, 1, 1, 1}, {1, 3, 1, 2}, {2}, {1, 2, 1, 1}}) {
                for (const auto value : {0.0, .5, 1.0}) {
                    const auto maze = generate_configured(extents, key, {.traversability = value}, budgets).value();
                    verify(maze);
                    if (maze.domain().cells() <= 2) {
                        CHECK(maze.measurements().strict_labyrinth);
                    }
                    if (maze.domain().cells() == 1) {
                        CHECK((maze.endpoints() == Endpoints{0, 0}));
                        CHECK(maze.measurements().degrees[0] == 1);
                        CHECK(maze.measurements().dead_ends == 0);
                    }
                }
            }
            std::array<Index, max_dimensions> extents{};
            extents.fill(1);
            extents.back() = 2;
            const auto highest = generate_configured(extents, key, {}, budgets).value();
            verify(highest);
            CHECK(ProcGenChecks::verify(maze_view(highest)).axis_edges[15] == 1);
        }
        void rounding() {
            const std::array<Index, 2> extents{4, 3};
            const auto domain = Domain::checked(extents).value();
            const double half = 0.5 / 65536;
            const auto down = configured_requirements(domain, {.traversability = std::nextafter(half, 0.0)}).value();
            const auto tie = configured_requirements(domain, {.traversability = half}).value();
            const auto up = configured_requirements(domain, {.traversability = std::nextafter(half, 1.0)}).value();
            CHECK(down.effective.ticks == 0);
            CHECK(tie.effective.ticks == 1);
            CHECK(up.effective.ticks == 1);
            CHECK(tie.effective.route_cells == 2);
            CHECK(configured_requirements(domain, {.traversability = std::nextafter(1.0, 0.0)})->effective.ticks == traversal_scale);
            CHECK(configured_requirements(domain, {.traversability = -0.0})->effective.ticks == 0);
            CHECK(configured_requirements(domain, {.traversability = std::numeric_limits<double>::denorm_min()})->effective.ticks == 0);
            CHECK((configured_requirements(domain, {.traversability = .5})->effective == EffectiveConfiguration{32768, 7}));
            // C=3: every quantized request below 1 has only two route cells.
            const std::array<Index, 1> three{3};
            const auto low = generate_configured(three, key, {.traversability = .99}, budgets).value();
            CHECK(low.main_route().size() == 2);
            CHECK(low.measurements().tree_branches->off_route_cells == 1);
            verify(low);
        }
        void fixed() {
            const std::array<Index, 2> square{2, 2};
            const auto automatic = generate_configured(square, key, {}, budgets).value();
            const auto ends = automatic.endpoints();
            const auto matched = generate_configured(square, key, {.endpoints = ends}, budgets).value();
            CHECK(same(automatic, matched));
            verify(matched);
            const auto backwards = generate_configured(square, key, {.endpoints = Endpoints{ends.goal, ends.entrance}}, budgets).value();
            CHECK(std::ranges::equal(backwards.main_route(), automatic.main_route() | std::views::reverse));
            CHECK(std::ranges::equal(backwards.passages(), automatic.passages()));
            verify(backwards);
            error_is(
                generate_configured(square, key, {.endpoints = Endpoints{0, 3}}, budgets), FailureKind::contradiction,
                Reason::endpoint_policy
            );
            error_is(
                generate_configured(square, key, {.endpoints = Endpoints{0, 0}}, budgets), FailureKind::contradiction,
                Reason::endpoint_policy
            );
            {
                Deny deny;
                error_is(
                    generate_configured(square, key, {.endpoints = Endpoints{0, 2}}, budgets), FailureKind::unsupported,
                    Reason::endpoint_policy
                );
            }
            error_is(
                generate_configured(square, key, {.endpoints = Endpoints{0, 4}}, budgets), FailureKind::invalid_input, Reason::endpoint
            );
            const std::array<Index, 2> odd{3, 3};
            error_is(
                generate_configured(odd, key, {.endpoints = Endpoints{1, 3}}, budgets), FailureKind::contradiction, Reason::endpoint_policy
            );
            error_is(
                generate_configured(odd, key, {.endpoints = Endpoints{0, 4}}, budgets), FailureKind::unsupported, Reason::endpoint_policy
            );
            const std::array<Index, 1> one{1};
            verify(generate_configured(one, key, {.endpoints = Endpoints{0, 0}}, budgets).value());
            // Fixed endpoints also preserve the selected intermediate route.
            const auto middle = generate_configured(square, key, {.traversability = .5}, budgets).value();
            verify(generate_configured(square, key, {.traversability = .5, .endpoints = middle.endpoints()}, budgets).value());
        }
        void invalid() {
            const std::array<Index, 2> extents{2, 3};
            for (double value :
                 {-1.0, 1.01, std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
                  std::numeric_limits<double>::quiet_NaN()}) {
                Deny deny;
                error_is(
                    generate_configured(extents, key, {.traversability = value}, budgets), FailureKind::invalid_input,
                    Reason::traversability
                );
            }
            {
                Deny deny;
                error_is(
                    generate_configured(extents, key, {.coverage = static_cast<Coverage>(77)}, budgets), FailureKind::unsupported,
                    Reason::coverage
                );
            }
            const std::array<Index, 1> zero{0};
            {
                Deny deny;
                error_is(generate_configured({}, key, {}, budgets), FailureKind::invalid_input, Reason::dimensions);
                error_is(generate_configured(zero, key, {}, budgets), FailureKind::invalid_input, Reason::extent);
            }
            std::array<Index, max_dimensions + 1> many{};
            many.fill(1);
            {
                Deny deny;
                error_is(generate_configured(many, key, {}, budgets), FailureKind::invalid_input, Reason::dimensions);
            }
        }
        void replay_case() {
            const std::array<Index, 2> extents{5, 3};
            const auto original = generate_configured(extents, key, {.traversability = .51}, budgets).value();
            const auto replayed = replay_configured(original.descriptor()).value();
            CHECK(same(original, replayed));
            CHECK(original.descriptor() == replayed.descriptor());
            CHECK(original.usage().work == replayed.usage().work);
            CHECK(original.usage().random_draws == replayed.usage().random_draws);
            auto descriptor = original.descriptor();
            descriptor.budgets = configured_hard_limits;
            CHECK(same(original, replay_configured(descriptor).value()));
            auto unrelated = IntegerStream::for_stage({key.seed, 0x63617665, key.stream});
            for (int i = 0; i < 1000; ++i) {
                static_cast<void>(unrelated.next());
            }
            CHECK(same(original, replay_configured(original.descriptor()).value()));
            descriptor.version = 2;
            {
                Deny deny;
                error_is(replay_configured(descriptor), FailureKind::invalid_input, Reason::version);
            }
            descriptor.version = 1;
            descriptor.algorithm = static_cast<ConfiguredAlgorithm>(77);
            {
                Deny deny;
                error_is(replay_configured(descriptor), FailureKind::invalid_input, Reason::algorithm);
            }
        }
        void ownership() {
            const std::array<Index, 2> extents{5, 3};
            const auto original = generate_configured(extents, key, {.traversability = .5}, budgets).value();
            auto copy = original;
            CHECK(copy.passages().data() != original.passages().data());
            CHECK(copy.main_route().data() != original.main_route().data());
            copy = generate_configured(extents, key, {}, budgets).value();
            CHECK(original.effective().route_cells == 8);
            CHECK(copy.effective().route_cells == 15);
            const auto snapshot = original;
            auto small = budgets;
            small.work = 0;
            error_is(generate_configured(extents, key, {}, small), FailureKind::budget_refused, Reason::work);
            CHECK(same(original, snapshot));
            CHECK(original.descriptor() == snapshot.descriptor());
            error_is(
                generate_configured(extents, key, {.traversability = -1}, budgets), FailureKind::invalid_input, Reason::traversability
            );
            error_is(
                generate_configured(extents, key, {.endpoints = Endpoints{0, 0}}, budgets), FailureKind::contradiction,
                Reason::endpoint_policy
            );
            const std::array<Index, 2> square{2, 2};
            error_is(
                generate_configured(square, key, {.endpoints = Endpoints{0, 2}}, budgets), FailureKind::unsupported, Reason::endpoint_policy
            );
            CHECK(same(original, snapshot));
            CHECK(original.descriptor() == snapshot.descriptor());
            bool propagated = false;
            try {
                Deny deny;
                static_cast<void>(generate_configured(extents, key, {}, budgets));
            } catch (const std::bad_alloc&) {
                propagated = true;
            }
            CHECK(propagated);
            CHECK(same(original, snapshot));
            ConfiguredMaze transferred = std::move(copy);
            verify(transferred);
            verify(original);
        }
        void budget_cases() {
            const std::array<Index, 2> extents{2, 3};
            const auto domain = Domain::checked(extents).value();
            const Configuration config{.traversability = .5};
            const auto needed = configured_requirements(domain, config).value();
            const auto measured = generate_configured(extents, key, config, budgets).value();
            Budgets exact{6, 5, needed.resources.storage_bytes, measured.usage().work};
            CHECK(same(measured, generate_configured(extents, key, config, exact).value()));
            for (const auto reason : {Reason::cells, Reason::edges, Reason::storage, Reason::work}) {
                auto lower = exact;
                if (reason == Reason::cells) {
                    --lower.cells;
                }
                if (reason == Reason::edges) {
                    --lower.edges;
                }
                if (reason == Reason::storage) {
                    --lower.storage_bytes;
                }
                if (reason == Reason::work) {
                    lower.work = needed.resources.minimum_work - 1;
                }
                Deny deny;
                error_is(generate_configured(extents, key, config, lower), FailureKind::budget_refused, reason);
            }
            const std::array<Index, 2> overflowing{maximum, 2};
            const std::array<Index, 1> byte_overflow{maximum};
            {
                Deny deny;
                error_is(generate_configured(overflowing, key, {}, budgets), FailureKind::invalid_input, Reason::overflow);
                error_is(
                    generate_configured(byte_overflow, key, {.traversability = 0}, budgets), FailureKind::invalid_input, Reason::overflow
                );
            }
            const std::array<Index, 1> cap{configured_hard_limits.cells + 1};
            {
                Deny deny;
                error_is(generate_configured(cap, key, {}, configured_hard_limits), FailureKind::budget_refused, Reason::cells);
            }
            CHECK(
                needed.resources.storage_bytes == configured_fixed_allowance + 4 * 6 + 8 * 4 + metric_requirements(domain)->storage_bytes
            );
        }
        void exhaustion() {
            const std::array<Index, 3> extents{2, 2, 2};
            const StreamKey rejecting{0xd78884aaeab763e4, 0, 0}; // first word 0 rejects bound 3 in the shuffle
            const auto complete = generate_configured(extents, rejecting, {.traversability = .5}, budgets).value();
            CHECK(complete.usage().random_draws > 2 + 3 + (8 - 5));
            CHECK(complete.usage().work >= configured_requirements(complete.domain(), {.traversability = .5})->resources.minimum_work);
            auto exact = budgets;
            exact.work = complete.usage().work;
            CHECK(same(complete, generate_configured(extents, rejecting, {.traversability = .5}, exact).value()));
            --exact.work;
            error_is(generate_configured(extents, rejecting, {.traversability = .5}, exact), FailureKind::budget_refused, Reason::work);
            const auto snapshot = complete;
            auto baseline = budgets;
            baseline.work = configured_requirements(complete.domain(), {.traversability = .5})->resources.minimum_work;
            error_is(generate_configured(extents, rejecting, {.traversability = .5}, baseline), FailureKind::budget_refused, Reason::work);
            CHECK(same(complete, snapshot));
            verify(complete);
        }
    }
    Counts run_configuration_cases(std::string_view selection) {
        struct Case {
            std::string_view name;
            void (*run)();
        };
        const std::array<Case, 10> cases{
            {{"config_profile", profile},
             {"config_four_d", four_d},
             {"config_tiny", tiny},
             {"config_rounding", rounding},
             {"config_fixed", fixed},
             {"config_invalid", invalid},
             {"config_replay", replay_case},
             {"config_ownership", ownership},
             {"config_limits", budget_cases},
             {"config_exhaustion", exhaustion}}
        };
        Counts counts;
        const auto initial = assertions;
        for (const auto& test : cases) {
            if (selection != "all" && selection != test.name) {
                continue;
            }
            const auto before = assertions;
            try {
                test.run();
                ++counts.passed;
                std::cout << "PASS " << test.name << ": " << assertions - before << " assertions\n";
            } catch (const std::exception& error) {
                ++counts.failed;
                std::cerr << "FAIL " << test.name << ": " << error.what() << '\n';
            }
        }
        counts.assertions = assertions - initial;
        return counts;
    }
}
