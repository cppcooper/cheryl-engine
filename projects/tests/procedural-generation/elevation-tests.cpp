#include "elevation.h"
#include "elevation-checks.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

namespace ElevationAllocation {
    thread_local bool denied = false;
}
void* operator new(std::size_t size) {
    if (ElevationAllocation::denied) {
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
    constexpr CaveBudgets budget{4096, 1024 * 1024, 2'000'000};
    constexpr StreamKey key{42, 0x67656f6772617068, 7};
    constexpr Height lowest = std::numeric_limits<Height>::min(), highest = std::numeric_limits<Height>::max();
    std::size_t assertions = 0;
    void check(bool ok, const char* expression, int line) {
        ++assertions;
        if (!ok) {
            throw std::runtime_error("line " + std::to_string(line) + ": " + expression);
        }
    }
#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)
    struct Deny {
        bool previous = ElevationAllocation::denied;
        Deny() { ElevationAllocation::denied = true; }
        ~Deny() { ElevationAllocation::denied = previous; }
    };
    template <class T> void error_is(const ElevationResult<T>& result, FailureKind kind, ElevationReason reason) {
        CHECK(!result);
        CHECK(result.error().kind == kind);
        CHECK(result.error().reason == reason);
    }
    bool same(const Elevation& a, const Elevation& b) {
        return a.descriptor().domain == b.descriptor().domain && a.descriptor().random == b.descriptor().random &&
               a.descriptor().requested == b.descriptor().requested && a.descriptor().algorithm == b.descriptor().algorithm &&
               a.descriptor().version == b.descriptor().version && a.measurements() == b.measurements() &&
               std::ranges::equal(a.heights(), b.heights());
    }
    Elevation make(std::array<Index, 2> extent, ElevationConfiguration config, StreamKey random = key) {
        return generate_elevation(extent, random, config, budget).value();
    }
    void verify(const Elevation& field) {
        ProcGenElevationChecks::inspect(field);
        const auto needed = elevation_requirements(field.descriptor().domain, field.effective()).value();
        CHECK(field.usage().work == 7 * needed.cells + 2 * needed.lattice_samples + field.usage().random_draws);
        CHECK(field.usage().work >= needed.minimum_work);
        CHECK(field.measurements().minimum >= field.effective().low);
        CHECK(field.measurements().maximum <= field.effective().high);
        CHECK(field.effective() == field.descriptor().requested);
    }
    void literal() {
        const auto field = make({4, 3}, {2, -7, 5});
        const std::array<Height, 12> expected{4, 1, -2, -3, 4, 2, 0, 0, 4, 3, 2, 3};
        CHECK(std::ranges::equal(field.heights(), expected));
        CHECK((field.measurements() == ElevationMeasurements{-3, 4, 7}));
        CHECK(field.height(3).value() == -3); // exact -2.5 floors to -3, not -2
        CHECK(field.descriptor().requested.low == -7 && field.descriptor().requested.high == 5);
        const auto plan = elevation_requirements(field.descriptor().domain, field.effective()).value();
        CHECK((plan.lattice == std::array<Index, 2>{3, 2})); // last x-anchor4 is outside output
        CHECK(field.usage().random_draws == 6);
        verify(field);
        for (Index index = 0; index < 12; ++index) {
            CHECK(field.height(index).value() == expected[static_cast<std::size_t>(index)]);
        }
    }
    void fields() {
        for (Index seed : {0U, 1U, 42U, 991U}) {
            for (Index spacing : {1U, 2U, 3U, 6U}) {
                const auto field = make({17, 11}, {spacing, -80, 120}, {seed, key.stage, key.stream});
                verify(field);
                const auto needed = elevation_requirements(field.descriptor().domain, field.effective()).value();
                CHECK(needed.lattice_samples <= needed.cells);
            }
        }
        const auto fine = make({17, 11}, {3, -80, 120}), coarse = make({17, 11}, {6, -80, 120});
        CHECK(elevation_requirements(fine.descriptor().domain, fine.effective())->lattice_samples == 35);
        CHECK(elevation_requirements(coarse.descriptor().domain, coarse.effective())->lattice_samples == 12);
        CHECK(!std::ranges::equal(fine.heights(), coarse.heights())); // this concrete fixture, no universal uniqueness
        const auto relief = make({17, 11}, {3, -240, 360});
        verify(relief);
        CHECK(relief.descriptor().requested.low == -240 && relief.descriptor().requested.high == 360);
        CHECK(relief.measurements().range > fine.measurements().range); // pinned seed/config evidence only
    }
    void sea() {
        const auto field = make({4, 3}, {2, -7, 5});
        const auto snapshot = field;
        const std::array<Height, 9> levels{-4, -3, -2, -1, 0, 1, 2, 3, 4};
        const std::array<Index, 9> waters{0, 1, 2, 2, 4, 5, 7, 9, 12};
        for (std::size_t i = 0; i < levels.size(); ++i) {
            const auto m = measure_sea(field, levels[i], 12).value();
            CHECK((m == SeaMeasurements{levels[i], 12 - waters[i], waters[i], 12}));
            Index counted = 0;
            for (const auto value : field.heights()) {
                counted += static_cast<Index>(classify_sea(value, levels[i]) == Surface::water);
            }
            CHECK(counted == waters[i]);
        }
        CHECK(classify_sea(-7, -7) == Surface::water);
        CHECK(classify_sea(-6, -7) == Surface::land);
        CHECK(measure_sea(field, lowest, 12)->water == 0);
        CHECK(measure_sea(field, highest, 12)->water == 12);
        {
            Deny deny;
            error_is(measure_sea(field, 0, 11), FailureKind::budget_refused, ElevationReason::work);
        }
        CHECK(same(field, snapshot));
        const auto coast = make({17, 11}, {3, -80, 120});
        CHECK(measure_sea(coast, 40, 187)->water > measure_sea(coast, 0, 187)->water);
    }
    void flat() {
        for (auto extent : {std::array<Index, 2>{1, 1}, {1, 7}, {8, 1}, {2, 2}}) {
            for (Height value : {lowest, Height{-7}, Height{0}, highest}) {
                const auto field = make(extent, {4096, value, value});
                verify(field);
                CHECK((field.measurements() == ElevationMeasurements{value, value, 0}));
                CHECK(std::ranges::all_of(field.heights(), [=](Height h) { return h == value; }));
                const auto needed = elevation_requirements(field.descriptor().domain, field.effective()).value();
                CHECK(field.usage().random_draws == needed.lattice_samples); // flat bound1 still draws
                CHECK(measure_sea(field, value, field.heights().size())->water == field.heights().size());
                if (value > lowest) {
                    CHECK(measure_sea(field, static_cast<Height>(value - 1), field.heights().size())->land == field.heights().size());
                }
            }
        }
        const auto full = make({2, 3}, {4096, lowest, highest});
        verify(full); // max offset/spacing weighted sum fits u64, signed oracle fits i64
        const auto direct = make({3, 2}, {1, lowest, highest});
        verify(direct);
        CHECK(direct.usage().random_draws == 6);
    }
    void replay() {
        CHECK(derive_stream_state(key) == UINT64_C(0x09722dbe78a416bc));
        auto stream = IntegerStream::for_stage({derive_stream_state(key), elevation_lattice_stage, 0});
        for (auto expected : {UINT64_C(0x94faeb5625057915), UINT64_C(0xeeec344ed751511c), UINT64_C(0x6671ca72b7982b2e)}) {
            CHECK(stream.next() == expected);
        }
        const auto field = make({17, 11}, {3, -80, 120});
        auto unrelated = IntegerStream::for_stage({derive_stream_state(key), 0xdead, 0});
        for (unsigned i = 0; i < 39; ++i) {
            static_cast<void>(unrelated.next());
        }
        CHECK(same(field, replay_elevation(field.descriptor()).value()));
        CHECK(same(field, make({17, 11}, {3, -80, 120})));
        auto generous = field.descriptor();
        generous.budgets = elevation_hard_limits;
        CHECK(same(field, replay_elevation(generous).value()));
        const auto descriptor = field.descriptor();
        static_cast<void>(measure_sea(field, -40, 187).value());
        CHECK(field.descriptor() == descriptor);
    }
    void invalid() {
        const auto d = Domain::checked(std::array<Index, 2>{3, 3}).value();
        const auto other = Domain::checked(std::array<Index, 3>{3, 1, 3}).value();
        {
            Deny deny;
            error_is(elevation_requirements(other, {1, 0, 1}), FailureKind::invalid_input, ElevationReason::dimensions);
        }
        for (auto config : {ElevationConfiguration{0, 0, 1}, {4097, 0, 1}, {1, 2, 1}}) {
            Deny deny;
            error_is(
                elevation_requirements(d, config), FailureKind::invalid_input,
                config.spacing == 1 ? ElevationReason::height_range : ElevationReason::spacing
            );
        }
        ElevationReplay desc{d, key, {1, 0, 1}, budget};
        desc.algorithm = static_cast<ElevationAlgorithm>(0);
        {
            Deny deny;
            error_is(replay_elevation(desc), FailureKind::unsupported, ElevationReason::algorithm);
        }
        desc.algorithm = ElevationAlgorithm::lattice_bilinear;
        for (auto version : {0U, 2U}) {
            desc.version = version;
            Deny deny;
            error_is(replay_elevation(desc), FailureKind::unsupported, ElevationReason::version);
        }
        {
            Deny deny;
            error_is(generate_elevation({}, key, {1, 0, 1}, budget), FailureKind::invalid_input, ElevationReason::dimensions);
        }
        {
            Deny deny;
            error_is(
                generate_elevation(std::array<Index, 2>{0, 2}, key, {1, 0, 1}, budget), FailureKind::invalid_input, ElevationReason::extent
            );
        }
        {
            Deny deny;
            error_is(
                generate_elevation(std::array<Index, 2>{std::numeric_limits<Index>::max(), 2}, key, {1, 0, 1}, budget),
                FailureKind::invalid_input, ElevationReason::overflow
            );
        }
        const auto huge = Domain::checked(std::array<Index, 2>{std::numeric_limits<Index>::max(), 1}).value();
        {
            Deny deny;
            error_is(elevation_requirements(huge, {4096, 0, 0}), FailureKind::invalid_input, ElevationReason::overflow);
        }
        const auto byte = Domain::checked(std::array<Index, 2>{std::numeric_limits<Index>::max() / 8 + 1, 1}).value();
        {
            Deny deny;
            error_is(elevation_requirements(byte, {1, 0, 0}), FailureKind::invalid_input, ElevationReason::overflow);
        }
        const auto field = make({3, 3}, {1, 0, 0});
        {
            Deny deny;
            error_is(field.height(9), FailureKind::invalid_input, ElevationReason::index);
        }
        {
            Deny deny;
            error_is(field.height(std::numeric_limits<Index>::max()), FailureKind::invalid_input, ElevationReason::index);
        }
    }
    void limits() {
        const auto field = make({4, 3}, {2, -7, 5});
        const auto needed = elevation_requirements(field.descriptor().domain, field.effective()).value();
        CHECK(needed.cells == 12 && needed.lattice_samples == 6 && needed.storage_bytes == 4168 && needed.minimum_work == 102);
        auto exact = field.descriptor();
        exact.budgets = {12, 4168, 102};
        CHECK(same(field, replay_elevation(exact).value()));
        for (auto reason : {ElevationReason::cells, ElevationReason::storage, ElevationReason::work}) {
            auto lower = exact;
            if (reason == ElevationReason::cells) {
                --lower.budgets.cells;
            }
            if (reason == ElevationReason::storage) {
                --lower.budgets.storage_bytes;
            }
            if (reason == ElevationReason::work) {
                --lower.budgets.work;
            }
            Deny deny;
            error_is(replay_elevation(lower), FailureKind::budget_refused, reason);
        }
        const auto rejected = make({1, 1}, {1, -1, 1}, {UINT64_C(0x0d201cfa683c7b1a), key.stage, key.stream});
        verify(rejected);
        CHECK(rejected.usage().random_draws == 2 && rejected.usage().work == 11);
        CHECK(rejected.height(0).value() == 0);
        auto tight = rejected.descriptor();
        tight.budgets.work = 11;
        CHECK(same(rejected, replay_elevation(tight).value()));
        --tight.budgets.work; // minimum10 admits, rejected raw0 costs an extra unit
        error_is(replay_elevation(tight), FailureKind::budget_refused, ElevationReason::work);
        const auto huge = Domain::checked(std::array<Index, 2>{elevation_hard_limits.cells + 1, 1}).value();
        {
            Deny deny;
            error_is(
                replay_elevation({huge, key, {4096, 0, 0}, elevation_hard_limits}), FailureKind::budget_refused, ElevationReason::cells
            );
        }
        const auto snapshot = field;
        {
            Deny deny;
            error_is(measure_sea(field, 0, 0), FailureKind::budget_refused, ElevationReason::work);
        }
        CHECK(same(field, snapshot));
    }
    void ownership() {
        const auto original = make({4, 3}, {2, -7, 5}), snapshot = original;
        auto copy = original;
        CHECK(copy.heights().data() != original.heights().data());
        copy = make({17, 11}, {6, -240, 360});
        CHECK(same(original, snapshot));
        bool propagated = false;
        try {
            Deny deny;
            static_cast<void>(replay_elevation(original.descriptor()));
        } catch (const std::bad_alloc&) {
            propagated = true;
        }
        CHECK(propagated && same(original, snapshot));
        auto refused = original.descriptor();
        refused.budgets.storage_bytes = 0;
        error_is(replay_elevation(refused), FailureKind::budget_refused, ElevationReason::storage);
        CHECK(same(original, snapshot));
        const auto lifetime = [] {
            const std::array<Index, 2> local{4, 3};
            const auto replay = ElevationReplay{Domain::checked(local).value(), key, {2, -7, 5}, budget};
            return replay_elevation(replay).value();
        }();
        CHECK(same(lifetime, original));
        const auto summary = [] {
            const auto local = make({4, 3}, {2, -7, 5});
            return measure_sea(local, 0, 12).value();
        }();
        CHECK((summary == SeaMeasurements{0, 8, 4, 12}));
        Elevation moved = std::move(copy);
        verify(moved);
    }
}
int main(int argc, char** argv) {
    struct Case {
        std::string_view name;
        void (*run)();
    };
    const std::array<Case, 8> cases{
        {{"elevation_literal", literal},
         {"elevation_fields", fields},
         {"elevation_sea", sea},
         {"elevation_flat", flat},
         {"elevation_replay", replay},
         {"elevation_invalid", invalid},
         {"elevation_limits", limits},
         {"elevation_ownership", ownership}}
    };
    const std::string_view selection = argc == 2 ? argv[1] : "all";
    if (argc > 2) {
        std::cerr << "Expected one elevation case name or all\n";
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
    std::cout << "Elevation cases: " << passed << " passed, " << failed << " failed, 0 skipped; " << assertions << " assertions\n";
    return passed != 0 && failed == 0 ? 0 : 1;
}
