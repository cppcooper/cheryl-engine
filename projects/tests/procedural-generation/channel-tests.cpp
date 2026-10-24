#include "channel-checks.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

namespace ChannelAllocation {
    thread_local bool denied = false;
}
void* operator new(std::size_t size) {
    if (ChannelAllocation::denied) {
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
    constexpr auto budget = channel_hard_limits;
    std::size_t assertions = 0;
    void check(bool ok, const char* expression, int line) {
        ++assertions;
        if (!ok) {
            throw std::runtime_error("line " + std::to_string(line) + ": " + expression);
        }
    }
#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)
    struct Deny {
        bool previous = ChannelAllocation::denied;
        Deny() { ChannelAllocation::denied = true; }
        ~Deny() { ChannelAllocation::denied = previous; }
    };
    template <class T> void error_is(const ChannelResult<T>& r, FailureKind kind, ChannelReason reason) {
        CHECK(!r);
        CHECK(r.error().kind == kind);
        CHECK(r.error().reason == reason);
    }
    Domain domain(Index w, Index h) {
        return Domain::checked(std::array<Index, 2>{w, h}).value();
    }
    struct Literal {
        Domain d;
        std::vector<Height> heights;
        std::vector<SpillCell> graph;
        std::vector<Index> outlets;
        ChannelInput input() const { return {d, heights, graph, outlets}; }
    };
    Literal fork_fixture() {
        Literal f{domain(3, 3), {8, 7, 8, 6, 5, 6, 8, 4, 8}, {}, {7}};
        const std::array<Index, 9> parents{3, 4, 5, 4, 7, 4, 7, no_receiver, 7}, ranks{5, 4, 6, 2, 1, 3, 7, 0, 8};
        for (std::size_t i = 0; i < 9; ++i) {
            f.graph.push_back({f.heights[i], f.heights[i], 0, parents[i], ranks[i], 7});
        }
        return f;
    }
    Literal lake_fixture(bool vertical = false) {
        Literal f{vertical ? domain(1, 5) : domain(5, 1), {5, 0, 0, 0, 0}, {}, {0}};
        for (Index i = 0; i < 5; ++i) {
            f.graph.push_back(
                {f.heights[static_cast<std::size_t>(i)], 5, i == 0 ? Index{0} : Index{5}, i == 0 ? no_receiver : i - 1, i, 0}
            );
        }
        return f;
    }
    bool same(const Channels& a, const Channels& b) {
        return a.descriptor().domain == b.descriptor().domain &&
               a.descriptor().minimum_contributing == b.descriptor().minimum_contributing &&
               a.descriptor().meaning == b.descriptor().meaning && a.descriptor().options == b.descriptor().options &&
               std::ranges::equal(a.routing(), b.routing()) && std::ranges::equal(a.contributing(), b.contributing()) &&
               std::ranges::equal(a.mask(), b.mask()) && std::ranges::equal(a.nodes(), b.nodes()) &&
               std::ranges::equal(a.edges(), b.edges()) && std::ranges::equal(a.path_cells(), b.path_cells()) &&
               a.measurements() == b.measurements();
    }
    Channels make(ChannelInput input, Index threshold) {
        auto r = extract_channels(input, threshold, budget).value();
        ProcGenChannelChecks::inspect(input, r);
        const auto p = channel_requirements(input.domain, input.outlets.size()).value();
        const auto m = r.measurements();
        CHECK(r.usage().work == p.minimum_work + 2 * m.selected_links + 3 * m.nodes + 2 * m.edges + 2 * m.path_points);
        CHECK(r.usage().work <= p.maximum_work);
        CHECK(r.usage().storage_bytes == p.base_storage_bytes + 16 * m.nodes + 40 * m.edges + 8 * m.path_points);
        CHECK(r.usage().storage_bytes <= p.maximum_storage_bytes);
        return r;
    }
    void literal() {
        const auto f = fork_fixture();
        const auto r = make(f.input(), 1);
        const std::array<Index, 9> units{1, 1, 1, 2, 6, 2, 1, 9, 1};
        const std::array<Index, 7> nodes{0, 1, 2, 4, 6, 7, 8};
        CHECK(std::ranges::equal(r.contributing(), units));
        CHECK((r.measurements() == ChannelMeasurements{9, 1, 9, 8, 0, 7, 6, 14, 9}));
        for (std::size_t n = 0; n < nodes.size(); ++n) {
            CHECK(r.nodes()[n].cell == nodes[n]);
        }
        CHECK(r.nodes()[3].roles == channel_junction);
        CHECK(r.nodes()[5].roles == (channel_junction | channel_outlet));
        const std::array<Index, 14> expected{0, 3, 4, 1, 4, 2, 5, 4, 4, 7, 6, 7, 8, 7};
        CHECK(std::ranges::equal(r.path_cells(), expected));
        const auto route = analyze_spill({f.d, f.heights, f.outlets}, spill_hard_limits).value();
        CHECK(std::ranges::equal(route.cells(), f.graph));
        CHECK(same(r, make(f.input(), 1)));
    }
    void lake() {
        for (bool vertical : {false, true}) {
            const auto f = lake_fixture(vertical);
            const auto r = make(f.input(), 2);
            const std::array<Index, 5> units{5, 4, 3, 2, 1};
            const std::array<std::uint8_t, 5> mask{1, 3, 3, 3, 0};
            const std::array<Index, 4> path{3, 2, 1, 0};
            CHECK(std::ranges::equal(r.contributing(), units));
            CHECK(std::ranges::equal(r.mask(), mask));
            CHECK(std::ranges::equal(r.path_cells(), path));
            CHECK((r.measurements() == ChannelMeasurements{5, 1, 4, 3, 3, 2, 1, 4, 5}));
            CHECK(r.edges()[0].fill_points == 3 && r.edges()[0].from == 1 && r.edges()[0].to == 0);
            const auto raw = analyze_drainage({f.d, f.heights, f.outlets}, drainage_hard_limits).value();
            CHECK(raw.measurements().outlet_contributing == 1 && raw.measurements().sinks == 4);
            CHECK(r.contributing()[0] == 5); // actual spill forest, not raw original-height units
        }
    }
    void thresholds() {
        const auto f = fork_fixture();
        const std::array<Index, 8> thresholds{1, 2, 3, 6, 7, 9, 10, no_receiver}, selected{9, 4, 2, 2, 1, 1, 0, 0};
        for (std::size_t k = 0; k < thresholds.size(); ++k) {
            const auto r = make(f.input(), thresholds[k]);
            CHECK(r.measurements().selected_cells == selected[k]);
            CHECK(r.measurements().terminal_contributing == 9);
            if (selected[k] == 0) {
                CHECK(r.nodes().empty() && r.edges().empty() && r.path_cells().empty());
            }
        }
        const auto a = make(f.input(), 2);
        CHECK((a.measurements() == ChannelMeasurements{9, 1, 4, 3, 0, 4, 3, 6, 9}));
        const auto b = make(f.input(), 9);
        CHECK(b.nodes().size() == 1 && b.nodes()[0].cell == 7 && b.nodes()[0].roles == (channel_source | channel_outlet));
        auto exact = budget;
        const auto p = channel_requirements(f.d, 1).value();
        exact.nodes = exact.edges = exact.path_points = 0;
        exact.storage_bytes = p.base_storage_bytes;
        exact.work = p.minimum_work;
        const auto empty = extract_channels(f.input(), 10, exact).value();
        CHECK(empty.usage().work == p.minimum_work && empty.usage().storage_bytes == p.base_storage_bytes);
    }
    void tiny() {
        for (Height height : {std::numeric_limits<Height>::min(), Height{0}, std::numeric_limits<Height>::max()}) {
            const std::array<Height, 1> h{height};
            const std::array<SpillCell, 1> graph{{{height, height, 0, no_receiver, 0, 0}}};
            const std::array<Index, 1> out{0};
            const auto r = make({domain(1, 1), h, graph, out}, 1);
            CHECK(r.nodes().size() == 1 && r.nodes()[0].roles == (channel_source | channel_outlet));
            CHECK(r.contributing()[0] == 1 && r.edges().empty() && r.path_cells().empty());
            CHECK(make({domain(1, 1), h, graph, out}, 2).mask()[0] == 0);
        }
        const std::array<Height, 4> h{0, 0, 0, 0};
        const std::array<SpillCell, 4> graph{
            {{0, 0, 0, no_receiver, 0, 0}, {0, 0, 0, 0, 2, 0}, {0, 0, 0, 3, 3, 3}, {0, 0, 0, no_receiver, 1, 3}}
        };
        const std::array<Index, 2> out{3, 0}, reversed{0, 3};
        const auto r = make({domain(2, 2), h, graph, out}, 2);
        CHECK(r.nodes().size() == 2 && r.edges().empty());
        CHECK(r.contributing()[0] == 2 && r.contributing()[3] == 2 && r.measurements().terminal_contributing == 4);
        CHECK(same(r, make({domain(2, 2), h, graph, reversed}, 2)));
        const auto high = std::numeric_limits<Height>::max(), low = std::numeric_limits<Height>::min();
        const std::array<Height, 3> extreme{high, low, low};
        const auto route = analyze_spill({domain(3, 1), extreme, std::array<Index, 1>{0}}, spill_hard_limits).value();
        const auto e = make({domain(3, 1), extreme, route.cells(), std::array<Index, 1>{0}}, 1);
        CHECK(e.routing()[1].depth == UINT64_C(4294967295) && e.edges()[0].fill_points == 2);
    }
    void generated() {
        for (Index seed : {0U, 1U, 42U, 991U}) {
            const auto field =
                generate_elevation(std::array<Index, 2>{17, 11}, {seed, 0x67656f6772617068, 7}, {3, -80, 120}, elevation_hard_limits)
                    .value();
            const auto copy = field;
            const std::array<Index, 2> out{0, 186};
            const auto route = analyze_spill({field.descriptor().domain, field.heights(), out}, spill_hard_limits).value();
            const auto saved = route;
            Index previous = 188;
            for (Index threshold : {1U, 2U, 8U, 24U, 188U}) {
                const auto r = make({field.descriptor().domain, field.heights(), route.cells(), out}, threshold);
                CHECK(r.measurements().selected_cells <= previous);
                previous = r.measurements().selected_cells;
                CHECK(std::ranges::equal(route.cells(), saved.cells()) && std::ranges::equal(field.heights(), copy.heights()));
            }
        }
    }
    void malformed() {
        const auto original = fork_fixture();
        for (unsigned bad = 0; bad < 6; ++bad) {
            auto f = original;
            if (bad == 0) {
                f.graph[0].parent = 7;
            } // noncardinal
            if (bad == 1) {
                f.graph[0].parent = 9;
            }
            if (bad == 2) {
                f.graph[8].rank = 5;
            } // duplicate rank
            if (bad == 3) {
                f.graph[8].rank = no_receiver;
            }
            if (bad == 4) {
                f.graph[0].parent = 0;
            }
            if (bad == 5) {
                f.graph[3].effective = 9;
                f.graph[3].depth = 3;
            } // uphill parent
            error_is(extract_channels(f.input(), 1, budget), FailureKind::invalid_input, ChannelReason::topology);
        }
        const std::array<Height, 3> h{0, 0, 0};
        const std::array<SpillCell, 3> cyclic{{{0, 0, 0, 1, 2, 2}, {0, 0, 0, 0, 1, 2}, {0, 0, 0, no_receiver, 0, 2}}};
        const std::array<Index, 1> out{2};
        error_is(extract_channels({domain(3, 1), h, cyclic, out}, 1, budget), FailureKind::invalid_input, ChannelReason::topology);
        auto f = original;
        f.graph[7].parent = 4;
        error_is(extract_channels(f.input(), 1, budget), FailureKind::invalid_input, ChannelReason::outlets);
        for (unsigned mismatch = 0; mismatch < 5; ++mismatch) {
            auto a = original;
            if (mismatch == 0) {
                ++a.heights[0];
            } // same size, incompatible exact source
            if (mismatch == 1) {
                ++a.graph[0].original;
            }
            if (mismatch == 2) {
                a.graph[0].effective = 7;
            }
            if (mismatch == 3) {
                a.graph[0].depth = 1;
            }
            if (mismatch == 4) {
                a.graph[0].outlet = 0;
            }
            error_is(extract_channels(a.input(), 1, budget), FailureKind::invalid_input, ChannelReason::source);
        }
    }
    void invalid() {
        const auto f = fork_fixture();
        {
            Deny deny;
            error_is(extract_channels(f.input(), 0, budget), FailureKind::invalid_input, ChannelReason::threshold);
        }
        auto input = f.input();
        input.meaning = static_cast<ChannelMeaning>(2);
        {
            Deny deny;
            error_is(extract_channels(input, 1, budget), FailureKind::unsupported, ChannelReason::algorithm);
        }
        input = f.input();
        input.heights = input.heights.first(8);
        {
            Deny deny;
            error_is(extract_channels(input, 1, budget), FailureKind::invalid_input, ChannelReason::shape);
        }
        input = f.input();
        input.routing = input.routing.first(8);
        {
            Deny deny;
            error_is(extract_channels(input, 1, budget), FailureKind::invalid_input, ChannelReason::shape);
        }
        input = f.input();
        input.outlets = {};
        {
            Deny deny;
            error_is(extract_channels(input, 1, budget), FailureKind::invalid_input, ChannelReason::outlets);
        }
        for (auto out : {std::array<Index, 2>{7, 7}, {7, 9}, {7, no_receiver}}) {
            input = f.input();
            input.outlets = out;
            Deny deny;
            error_is(extract_channels(input, 1, budget), FailureKind::invalid_input, ChannelReason::outlets);
        }
        const auto wrong = Domain::checked(std::array<Index, 3>{3, 3, 1}).value();
        input = f.input();
        input.domain = wrong;
        {
            Deny deny;
            error_is(extract_channels(input, 1, budget), FailureKind::invalid_input, ChannelReason::dimensions);
        }
        for (auto version : {0U, 2U}) {
            Deny deny;
            error_is(
                extract_channels(f.input(), 1, budget, {ChannelAlgorithm::threshold_runs, version}), FailureKind::unsupported,
                ChannelReason::version
            );
        }
        {
            Deny deny;
            error_is(
                extract_channels(f.input(), 1, budget, {static_cast<ChannelAlgorithm>(0), 1}), FailureKind::unsupported,
                ChannelReason::algorithm
            );
        }
        const auto huge = domain(std::numeric_limits<Index>::max(), 1);
        {
            Deny deny;
            error_is(channel_requirements(huge, 1), FailureKind::invalid_input, ChannelReason::overflow);
        }
        {
            Deny deny;
            error_is(channel_requirements(f.d, 10), FailureKind::invalid_input, ChannelReason::outlets);
        }
        CHECK(!Domain::checked({}) && !Domain::checked(std::array<Index, 2>{0, 1}));
        CHECK(!Domain::checked(std::array<Index, 2>{std::numeric_limits<Index>::max(), 2}));
    }
    void limits() {
        const auto f = fork_fixture();
        const auto r = make(f.input(), 2), saved = r;
        const auto p = channel_requirements(f.d, 1).value();
        CHECK(p.base_storage_bytes == 12882 && r.usage().storage_bytes == 13114);
        const auto m = r.measurements();
        const ChannelBudgets exact{9, m.nodes, m.edges, m.path_points, r.usage().storage_bytes, r.usage().work};
        CHECK(same(r, extract_channels(f.input(), 2, exact).value()));
        for (auto reason :
             {ChannelReason::cells, ChannelReason::nodes, ChannelReason::edges, ChannelReason::paths, ChannelReason::storage,
              ChannelReason::work}) {
            auto lower = exact;
            if (reason == ChannelReason::cells) {
                --lower.cells;
            }
            if (reason == ChannelReason::nodes) {
                --lower.nodes;
            }
            if (reason == ChannelReason::edges) {
                --lower.edges;
            }
            if (reason == ChannelReason::paths) {
                --lower.path_points;
            }
            if (reason == ChannelReason::storage) {
                --lower.storage_bytes;
            }
            if (reason == ChannelReason::work) {
                --lower.work;
            }
            error_is(extract_channels(f.input(), 2, lower), FailureKind::budget_refused, reason);
            CHECK(same(r, saved));
        }
        for (auto reason : {ChannelReason::storage, ChannelReason::work}) {
            auto early = exact;
            if (reason == ChannelReason::storage) {
                early.storage_bytes = p.base_storage_bytes - 1;
            } else {
                early.work = p.minimum_work - 1;
            }
            Deny deny;
            error_is(extract_channels(f.input(), 2, early), FailureKind::budget_refused, reason);
        }
        for (Index work = p.minimum_work; work < r.usage().work; ++work) {
            auto lower = exact;
            lower.work = work;
            error_is(extract_channels(f.input(), 2, lower), FailureKind::budget_refused, ChannelReason::work);
            CHECK(same(r, saved));
        }
        const auto too_large = domain(channel_hard_limits.cells + 1, 1);
        {
            Deny deny;
            error_is(
                extract_channels({too_large, {}, {}, std::array<Index, 1>{0}}, 1, budget), FailureKind::budget_refused, ChannelReason::cells
            );
        }
        const Index c = 65536;
        const auto d = domain(1, c);
        const std::vector<Height> h(static_cast<std::size_t>(c), 0);
        std::vector<SpillCell> route(static_cast<std::size_t>(c));
        for (Index i = 0; i < c; ++i) {
            route[static_cast<std::size_t>(i)] = {0, 0, 0, i == 0 ? no_receiver : i - 1, i, 0};
        }
        const auto maximum = extract_channels({d, h, route, std::array<Index, 1>{0}}, 1, budget).value();
        CHECK(maximum.nodes().size() == 2 && maximum.edges().size() == 1 && maximum.path_cells().size() == c);
        CHECK(maximum.contributing()[0] == c && maximum.path_cells().front() == c - 1 && maximum.path_cells().back() == 0);
        CHECK(maximum.usage().work <= channel_requirements(d, 1)->maximum_work);
    }
    void ownership() {
        auto f = lake_fixture();
        const auto r = make(f.input(), 2), saved = r;
        CHECK(r.routing().data() != f.graph.data() && r.routing().data() != saved.routing().data());
        CHECK(r.mask().data() != saved.mask().data() && r.path_cells().data() != saved.path_cells().data());
        const auto lifetime = [] {
            const auto local = lake_fixture();
            return extract_channels(local.input(), 2, budget).value();
        }();
        CHECK(same(r, lifetime));
        f.heights[1] = 9;
        f.graph[1].original = 9;
        CHECK(same(r, saved));
        auto invalid = lake_fixture();
        invalid.graph[0].rank = 4;
        error_is(extract_channels(invalid.input(), 2, budget), FailureKind::invalid_input, ChannelReason::topology);
        CHECK(same(r, saved));
        bool propagated = false;
        const auto valid = lake_fixture();
        try {
            Deny deny;
            static_cast<void>(extract_channels(valid.input(), 2, budget));
        } catch (const std::bad_alloc&) {
            propagated = true;
        }
        CHECK(propagated && same(r, saved));
        auto copy = r;
        copy = make(valid.input(), 1);
        Channels moved = std::move(copy);
        CHECK(moved.measurements().selected_cells == 5 && same(r, saved));
    }
}
int main(int argc, char** argv) {
    struct Case {
        std::string_view name;
        void (*run)();
    };
    const std::array<Case, 9> cases{
        {{"channel_literal", literal},
         {"channel_lake", lake},
         {"channel_thresholds", thresholds},
         {"channel_tiny", tiny},
         {"channel_generated", generated},
         {"channel_malformed", malformed},
         {"channel_invalid", invalid},
         {"channel_limits", limits},
         {"channel_ownership", ownership}}
    };
    const std::string_view selection = argc == 2 ? argv[1] : "all";
    if (argc > 2) {
        std::cerr << "Expected one channel case name or all\n";
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
    std::cout << "Channel cases: " << passed << " passed, " << failed << " failed, 0 skipped; " << assertions << " assertions\n";
    return passed != 0 && failed == 0 ? 0 : 1;
}
