#include "tunnels.h"
#include "tunnel-checks.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

namespace TunnelAllocation {
    thread_local bool denied = false;
}
void* operator new(std::size_t size) {
    if (TunnelAllocation::denied) {
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
    constexpr CaveBudgets budget{4096, 1024 * 1024, 8'000'000};
    std::size_t assertions = 0;
    void check(bool ok, const char* expression, int line) {
        ++assertions;
        if (!ok) {
            throw std::runtime_error("line " + std::to_string(line) + ": " + expression);
        }
    }
#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)
    struct Deny {
        bool previous = TunnelAllocation::denied;
        Deny() { TunnelAllocation::denied = true; }
        ~Deny() { TunnelAllocation::denied = previous; }
    };
    template <class T> void error_is(const Result<T>& value, FailureKind kind, Reason reason) {
        CHECK(!value);
        CHECK(value.error().kind == kind);
        CHECK(value.error().reason == reason);
    }
    bool same(const ConnectedCave& a, const ConnectedCave& b) {
        return a.descriptor().domain == b.descriptor().domain && a.descriptor().version == b.descriptor().version &&
               a.measurements() == b.measurements() && std::ranges::equal(a.source(), b.source()) &&
               std::ranges::equal(a.occupancy(), b.occupancy()) && std::ranges::equal(a.connections(), b.connections());
    }
    void verify(OccupancyView input, const ConnectedCave& result) {
        ProcGenTunnelChecks::inspect(input, result);
        const auto needed = tunnel_requirements(input.domain).value();
        CHECK(result.usage().work >= needed.minimum_work);
        CHECK(result.usage().work <= budget.work);
        CHECK(result.measurements().carved <= input.domain.cells());
        CHECK(result.usage().stale_pops <= result.usage().pops);
        if (!result.connections().empty() && input.domain.cells() <= 256) {
            CHECK(result.connections().front().carved == ProcGenTunnelChecks::first_cost(input));
        }
    }
    std::vector<std::uint8_t> islands(const Domain& d, std::initializer_list<Index> floors) {
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(d.cells()), 1);
        for (auto cell : floors) {
            bytes.at(static_cast<std::size_t>(cell)) = 0;
        }
        return bytes;
    }
    void literal() {
        const auto d = Domain::checked(std::array<Index, 2>{7, 3}).value();
        const auto source = islands(d, {8, 12});
        const auto snapshot = source;
        const auto old = analyze_clearance({d, source}, 1, budget).value();
        const auto result = connect_cave({d, source}, budget).value();
        verify({d, source}, result);
        const auto expected = islands(d, {8, 9, 10, 11, 12});
        CHECK(std::ranges::equal(result.occupancy(), expected));
        CHECK((result.measurements() == TunnelMeasurements{2, 2, 5, 1, 3, 1, 4, true}));
        CHECK((result.connections()[0] == TunnelConnection{8, 12, 3, 4}));
        CHECK(source == snapshot);
        const auto fresh = analyze_clearance({d, result.occupancy()}, 1, budget).value();
        CHECK(old.measurements().usable == 2);
        CHECK(fresh.measurements().usable == 5);
        CHECK(old.distances()[9] == 0 && fresh.distances()[9] == 1);
        CHECK(old.distances()[9] == 0); // no stale-input alias or update
        CHECK(result.descriptor().version == 1);
    }
    void multiple() {
        const auto d = Domain::checked(std::array<Index, 2>{7, 5}).value();
        const auto source = islands(d, {8, 12, 24});
        const auto result = connect_cave({d, source}, budget).value();
        verify({d, source}, result);
        CHECK((result.measurements() == TunnelMeasurements{3, 3, 7, 1, 4, 2, 6, true}));
        const std::array<TunnelConnection, 2> expected{{{8, 12, 3, 4}, {10, 24, 1, 2}}};
        CHECK(std::ranges::equal(result.connections(), expected));
        CHECK(std::ranges::equal(result.occupancy(), islands(d, {8, 9, 10, 11, 12, 17, 24})));
        const auto square = Domain::checked(std::array<Index, 2>{5, 5}).value();
        const auto diagonal = islands(square, {6, 12});
        CHECK(ProcGenTunnelChecks::count({square, diagonal}).components.size() == 2);
        const auto joined = connect_cave({square, diagonal}, budget).value();
        verify({square, diagonal}, joined);
        CHECK((joined.connections()[0] == TunnelConnection{6, 12, 1, 2}));
        CHECK(joined.occupancy()[7] == 0 && joined.occupancy()[11] == 1); // tie: low index7
        // A first connection may absorb more than one chamber; no record-count
        // equality with initial-components-minus-one is promised.
        const auto three = islands(square, {6, 8, 12});
        const auto merged = connect_cave({square, three}, budget).value();
        verify({square, three}, merged);
        CHECK(merged.measurements().original_components == 3);
        CHECK(merged.measurements().connections == 1 && merged.measurements().carved == 1);
        CHECK(merged.occupancy()[7] == 0);
        const auto many = islands(square, {6, 8, 16, 18});
        const auto joined_many = connect_cave({square, many}, budget).value();
        verify({square, many}, joined_many);
        CHECK(joined_many.measurements().connections <= 3);
    }
    void exhaustive() {
        const auto d = Domain::checked(std::array<Index, 2>{5, 5}).value();
        for (unsigned bits = 0; bits < 512; ++bits) {
            std::vector<std::uint8_t> source(25, 1);
            for (unsigned bit = 0; bit < 9; ++bit) {
                if (((bits >> bit) & 1U) != 0) {
                    source[static_cast<std::size_t>((bit / 3 + 1) * 5 + bit % 3 + 1)] = 0;
                }
            }
            const auto snapshot = source;
            const auto result = connect_cave({d, source}, budget).value();
            verify({d, source}, result);
            CHECK(source == snapshot);
            CHECK(same(result, connect_cave({d, source}, budget).value()));
            CHECK(result.measurements().original_floor == ProcGenTunnelChecks::count({d, source}).floor);
            if (result.measurements().original_components <= 1) {
                CHECK(result.measurements().carved == 0);
                CHECK(std::ranges::equal(result.occupancy(), source));
            }
        }
    }
    void generated() {
        std::size_t disconnected = 0;
        const std::array<Index, 2> extents{17, 11};
        for (auto seed : {0U, 42U, 991U, 6543U}) {
            const auto cave = generate_cave(extents, {seed, 0x63617665, 7}, {.solid_fill = 6500, .steps = 0}, budget).value();
            const auto descriptor = cave.descriptor();
            const std::vector<std::uint8_t> snapshot(cave.occupancy().begin(), cave.occupancy().end());
            const auto joined = connect_cave({cave.domain(), cave.occupancy()}, budget).value();
            verify({cave.domain(), cave.occupancy()}, joined);
            disconnected += static_cast<std::size_t>(joined.measurements().original_components > 1);
            auto unrelated = IntegerStream::for_stage({seed, 0xdead, 13});
            for (unsigned i = 0; i < 17; ++i) {
                static_cast<void>(unrelated.next());
            }
            CHECK(same(joined, connect_cave({cave.domain(), cave.occupancy()}, tunnel_hard_limits).value()));
            CHECK(cave.descriptor() == descriptor);
            CHECK(std::ranges::equal(cave.occupancy(), snapshot));
            // Replay transformation from its complete exact base, not a seed.
            CHECK(same(joined, connect_cave({joined.descriptor().domain, joined.source()}, budget, joined.descriptor().version).value()));
            const auto fresh = analyze_clearance({joined.descriptor().domain, joined.occupancy()}, 1, budget).value();
            CHECK(fresh.measurements().usable == joined.measurements().floor);
            CHECK(fresh.measurements().components == joined.measurements().components);
        }
        CHECK(disconnected > 0);
    }
    void empty() {
        for (auto shape : {std::array<Index, 2>{1, 1}, {1, 7}, {8, 1}, {2, 6}, {3, 3}, {5, 5}}) {
            const auto d = Domain::checked(shape).value();
            const auto source = islands(d, {});
            const auto result = connect_cave({d, source}, budget).value();
            verify({d, source}, result);
            CHECK((result.measurements() == TunnelMeasurements{}));
            CHECK(result.usage().work == 8 * d.cells());
            CHECK(result.connections().empty());
            CHECK(result.usage().pushes == 0 && result.usage().peak_queue == 0);
        }
        for (auto shape : {std::array<Index, 2>{3, 3}, {6, 5}}) {
            const auto d = Domain::checked(shape).value();
            auto source = islands(d, {});
            for (Index y = 1; y + 1 < shape[1]; ++y) {
                for (Index x = 1; x + 1 < shape[0]; ++x) {
                    source[static_cast<std::size_t>(y * shape[0] + x)] = 0;
                }
            }
            const auto result = connect_cave({d, source}, budget).value();
            verify({d, source}, result);
            CHECK(result.measurements().original_components == 1);
            CHECK(result.measurements().connected);
            CHECK(result.measurements().carved == 0 && result.connections().empty());
            CHECK(std::ranges::equal(result.occupancy(), source));
            CHECK(result.usage().work == 8 * d.cells() + 6 * result.measurements().floor);
        }
    }
    void invalid() {
        const auto d = Domain::checked(std::array<Index, 2>{5, 5}).value();
        const auto other = Domain::checked(std::array<Index, 3>{5, 1, 5}).value();
        auto source = islands(d, {6, 18});
        {
            Deny deny;
            error_is(connect_cave({d, source}, budget, 0), FailureKind::unsupported, Reason::tunnel_version);
            error_is(connect_cave({d, source}, budget, 2), FailureKind::unsupported, Reason::tunnel_version);
            error_is(connect_cave({other, source}, budget), FailureKind::invalid_input, Reason::tunnel_dimensions);
            error_is(connect_cave({d, {}}, budget), FailureKind::invalid_input, Reason::occupancy);
            error_is(connect_cave({d, std::span(source).first(24)}, budget), FailureKind::invalid_input, Reason::occupancy);
        }
        for (std::uint8_t value : {std::uint8_t{2}, std::uint8_t{255}}) {
            source[24] = value;
            Deny deny;
            error_is(connect_cave({d, source}, budget), FailureKind::invalid_input, Reason::occupancy);
        }
        source[24] = 1;
        source.push_back(1);
        {
            Deny deny;
            error_is(connect_cave({d, source}, budget), FailureKind::invalid_input, Reason::occupancy);
        }
        source.pop_back();
        for (std::size_t cell : {0U, 2U, 4U, 10U, 14U, 20U, 22U, 24U}) {
            source[cell] = 0;
            {
                Deny deny;
                error_is(connect_cave({d, source}, budget), FailureKind::contradiction, Reason::tunnel_boundary);
            }
            source[cell] = 1;
        }
        const auto huge = Domain::checked(std::array<Index, 2>{std::numeric_limits<Index>::max() / 139 + 1, 1}).value();
        const auto product = Domain::checked(std::array<Index, 2>{std::numeric_limits<Index>::max(), 2});
        CHECK(!product && product.error().reason == Reason::overflow);
        CHECK(!Domain::checked(std::array<Index, 2>{0, 2}));
        CHECK(!Domain::checked({}));
        {
            Deny deny;
            error_is(tunnel_requirements(huge), FailureKind::invalid_input, Reason::overflow);
        }
    }
    void limits() {
        const auto d = Domain::checked(std::array<Index, 2>{7, 3}).value();
        const auto solid = islands(d, {}), pair = islands(d, {8, 12});
        const auto needed = tunnel_requirements(d).value();
        CHECK(needed.cells == 21 && needed.storage_bytes == 7015);
        CHECK(needed.minimum_work == 168 && needed.pushes_per_search == 105);
        const CaveBudgets exact{21, 7015, 168};
        CHECK(connect_cave({d, solid}, exact).value().usage().work == 168);
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
            error_is(connect_cave({d, solid}, lower), FailureKind::budget_refused, reason);
        }
        const auto joined = connect_cave({d, pair}, budget).value();
        const auto snapshot = joined;
        auto tight = exact;
        tight.work = joined.usage().work;
        CHECK(tight.work > needed.minimum_work);
        CHECK(same(joined, connect_cave({d, pair}, tight).value()));
        --tight.work;
        error_is(connect_cave({d, pair}, tight), FailureKind::budget_refused, Reason::work);
        // Lower bound allows initialization; a search/flood budget can still run
        // out. Expected failure never publishes the carved unpublished buffer.
        tight.work = needed.minimum_work;
        error_is(connect_cave({d, pair}, tight), FailureKind::budget_refused, Reason::work);
        CHECK(same(joined, snapshot));
        CHECK(pair == islands(d, {8, 12}));
        const auto wide = Domain::checked(std::array<Index, 2>{tunnel_hard_limits.cells + 1, 1}).value();
        const auto oversized = islands(wide, {}); // 64KiB, not host memory pressure
        {
            Deny deny;
            error_is(connect_cave({wide, oversized}, tunnel_hard_limits), FailureKind::budget_refused, Reason::cells);
        }
        verify({d, pair}, joined);
    }
    void ownership() {
        const auto d = Domain::checked(std::array<Index, 2>{7, 3}).value();
        auto source = islands(d, {8, 12});
        const auto original = connect_cave({d, source}, budget).value();
        const auto snapshot = original;
        auto copy = original;
        CHECK(copy.source().data() != original.source().data());
        CHECK(copy.occupancy().data() != original.occupancy().data());
        CHECK(copy.connections().data() != original.connections().data());
        source[8] = 1;
        copy = connect_cave({d, source}, budget).value();
        CHECK(copy.measurements().carved == 0);
        CHECK(original.source()[8] == 0 && original.occupancy()[8] == 0);
        CHECK(same(original, snapshot));
        auto small = budget;
        small.work = 0;
        error_is(connect_cave({d, source}, small), FailureKind::budget_refused, Reason::work);
        source[0] = 0;
        error_is(connect_cave({d, source}, budget), FailureKind::contradiction, Reason::tunnel_boundary);
        source[0] = 1;
        bool propagated = false;
        try {
            Deny deny;
            static_cast<void>(connect_cave({d, source}, budget));
        } catch (const std::bad_alloc&) {
            propagated = true;
        }
        CHECK(propagated);
        CHECK(same(original, snapshot));
        const auto lifetime = [] {
            const auto local = Domain::checked(std::array<Index, 2>{7, 3}).value();
            const auto bytes = islands(local, {8, 12});
            return connect_cave({local, bytes}, budget).value();
        }();
        CHECK(same(lifetime, original));
        ConnectedCave moved = std::move(copy);
        verify({d, source}, moved);
    }
}
int main(int argc, char** argv) {
    struct Case {
        std::string_view name;
        void (*run)();
    };
    const std::array<Case, 8> cases{
        {{"tunnel_literal", literal},
         {"tunnel_multiple", multiple},
         {"tunnel_exhaustive", exhaustive},
         {"tunnel_generated", generated},
         {"tunnel_empty", empty},
         {"tunnel_invalid", invalid},
         {"tunnel_limits", limits},
         {"tunnel_ownership", ownership}}
    };
    const std::string_view selection = argc == 2 ? argv[1] : "all";
    if (argc > 2) {
        std::cerr << "Expected one tunnel case name or all\n";
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
    std::cout << "Tunnel cases: " << passed << " passed, " << failed << " failed, 0 skipped; " << assertions << " assertions\n";
    return failed == 0 && passed != 0 ? 0 : 1;
}
