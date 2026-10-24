#include "dungeon.h"
#include "dungeon-checks.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

namespace DungeonAllocation {
    thread_local bool denied = false;
}
void* operator new(std::size_t size) {
    if (DungeonAllocation::denied) {
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
    constexpr StreamKey key{42, 0x64756e67656f6e, 7};
    constexpr std::array<Index, 2> shape{31, 21};
    constexpr DungeonConfiguration small{8, {2, 2}, {4, 4}, 1, 1, 256};
    constexpr DungeonConfiguration large{4, {5, 5}, {7, 7}, 2, 2, 256};
    std::size_t assertions = 0;
    void check(bool ok, const char* expression, int line) {
        ++assertions;
        if (!ok) {
            throw std::runtime_error("line " + std::to_string(line) + ": " + expression);
        }
    }
#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)
    struct Deny {
        bool previous = DungeonAllocation::denied;
        Deny() { DungeonAllocation::denied = true; }
        ~Deny() { DungeonAllocation::denied = previous; }
    };
    template <class T> void error_is(const Result<T>& value, FailureKind kind, Reason reason) {
        CHECK(!value);
        CHECK(value.error().kind == kind);
        CHECK(value.error().reason == reason);
    }
    bool same(const Dungeon& a, const Dungeon& b) {
        return a.descriptor().domain == b.descriptor().domain && a.descriptor().random == b.descriptor().random &&
               a.descriptor().requested == b.descriptor().requested && a.descriptor().algorithm == b.descriptor().algorithm &&
               a.descriptor().version == b.descriptor().version && a.measurements() == b.measurements() &&
               std::ranges::equal(a.rooms(), b.rooms()) && std::ranges::equal(a.connections(), b.connections()) &&
               std::ranges::equal(a.occupancy(), b.occupancy());
    }
    void verify(const Dungeon& dungeon) {
        const auto report = ProcGenDungeonChecks::inspect(dungeon);
        CHECK(report.floor == dungeon.measurements().floor);
        CHECK(report.components == (dungeon.rooms().empty() ? 0U : 1U));
        CHECK(dungeon.usage().work >= dungeon_requirements(dungeon.descriptor().domain, dungeon.effective())->minimum_work);
        CHECK(dungeon.usage().random_draws <= dungeon.usage().work);
        CHECK(dungeon.descriptor().version == 1);
    }
    Dungeon make(std::array<Index, 2> extents, DungeonConfiguration config, StreamKey random = key) {
        return generate_dungeon(extents, random, config, budget).value();
    }
    void literal() {
        const auto one = make({5, 5}, {1, {3, 3}, {3, 3}, 0, 1, 1});
        verify(one);
        const std::array<DungeonRoom, 1> expected{{{0, {1, 1}, {4, 4}}}};
        CHECK(std::ranges::equal(one.rooms(), expected));
        CHECK(one.measurements().room_floor == 9 && one.measurements().corridor_floor == 0);
        CHECK(one.measurements().attempts == 1 && one.usage().random_draws == 4);
        for (Index y = 0; y < 5; ++y) {
            for (Index x = 0; x < 5; ++x) {
                CHECK(
                    one.occupancy()[static_cast<std::size_t>(y * 5 + x)] == static_cast<std::uint8_t>(x == 0 || y == 0 || x == 4 || y == 4)
                );
            }
        }
        // Python integer-spec vectors pin independent child stages/placements.
        const auto three = make({5, 3}, {3, {1, 1}, {1, 1}, 0, 1, 8});
        verify(three);
        const std::array<DungeonRoom, 3> rooms{{{0, {2, 1}, {3, 2}}, {1, {3, 1}, {4, 2}}, {2, {1, 1}, {2, 2}}}};
        const std::array<DungeonConnection, 2> links{{{0, 1, 1, 1, 0}, {1, 2, 0, 2, 0}}};
        CHECK(std::ranges::equal(three.rooms(), rooms));
        CHECK(std::ranges::equal(three.connections(), links));
        CHECK(three.measurements().achievement == DungeonAchievement::complete);
        CHECK(three.measurements().floor == 3 && three.measurements().corridor_floor == 0);
        CHECK(three.usage().random_draws == 14);
        // Second corridor crosses room0: logical graph is a chain; physical
        // floor adjacency need not reproduce that graph's individual edges.
        CHECK(three.occupancy()[7] == 0);
    }
    void layout() {
        Index with_corridors = 0;
        for (Index seed : {0U, 1U, 42U, 991U, 6543U}) {
            for (auto config : {small, large}) {
                const auto dungeon = make(shape, config, {seed, key.stage, key.stream});
                verify(dungeon);
                CHECK(dungeon.measurements().requested == config.rooms);
                CHECK(dungeon.rooms().size() <= config.rooms);
                CHECK(dungeon.connections().size() == (dungeon.rooms().empty() ? 0 : dungeon.rooms().size() - 1));
                with_corridors += static_cast<Index>(dungeon.measurements().corridor_floor != 0);
            }
        }
        CHECK(with_corridors > 0);
        // Unequal/tiny interior geometry with nontrivial separation policies.
        for (Index seed = 0; seed < 6; ++seed) {
            for (Index spacing : {0U, 1U, 2U}) {
                const auto d = make({8, 5}, {6, {1, 1}, {2, 2}, spacing, 1, 24}, {seed, key.stage, 3});
                verify(d);
            }
        }
    }
    void partial() {
        const auto packed = make({5, 5}, {2, {3, 3}, {3, 3}, 0, 1, 4});
        verify(packed);
        CHECK(packed.measurements().achievement == DungeonAchievement::attempt_limit);
        CHECK(packed.measurements().requested == 2 && packed.measurements().achieved == 1);
        CHECK(packed.measurements().attempts == 4 && packed.measurements().overlap_rejections == 3);
        CHECK(packed.measurements().fit_rejections == 0 && packed.measurements().connected);
        const auto zero = make({5, 5}, {2, {1, 1}, {2, 2}, 1, 1, 0});
        verify(zero);
        CHECK(zero.rooms().empty() && !zero.measurements().connected);
        CHECK(zero.measurements().achievement == DungeonAchievement::attempt_limit);
        for (auto extents : {std::array<Index, 2>{1, 1}, {1, 8}, {2, 6}, {5, 5}}) {
            const auto empty = make(extents, {0, {1, 1}, {1, 1}, 0, std::numeric_limits<Index>::max(), 0});
            verify(empty);
            CHECK(empty.measurements().achievement == DungeonAchievement::complete);
            CHECK(empty.measurements().floor == 0 && empty.usage().random_draws == 0);
        }
        const auto domain = Domain::checked(std::array<Index, 2>{1, 1}).value();
        {
            Deny deny;
            error_is(dungeon_requirements(domain, {1, {1, 1}, {1, 1}, 0, 1, 8}), FailureKind::contradiction, Reason::dungeon_fit);
        }
        const auto larger = Domain::checked(std::array<Index, 2>{7, 7}).value();
        {
            Deny deny;
            error_is(dungeon_requirements(larger, {1, {2, 1}, {3, 2}, 0, 3, 8}), FailureKind::contradiction, Reason::dungeon_fit);
        }
        {
            Deny deny;
            error_is(
                dungeon_requirements(larger, {1, {1, 1}, {1, 1}, 0, std::numeric_limits<Index>::max(), 8}), FailureKind::contradiction,
                Reason::dungeon_fit
            );
        }
    }
    void controls() {
        const auto one = make(shape, {1, {1, 1}, {1, 1}, 0, 1, 64});
        const auto four = make(shape, {4, {1, 1}, {1, 1}, 0, 1, 64});
        verify(one);
        verify(four);
        CHECK(one.rooms().size() == 1 && four.rooms().size() == 4);
        CHECK(one.rooms()[0] == four.rooms()[0]); // room selection is independent of route draws
        CHECK(one.measurements().room_floor == 1 && four.measurements().room_floor == 4);
        const auto nine = make(shape, {1, {3, 3}, {3, 3}, 0, 1, 64});
        verify(nine);
        CHECK(nine.measurements().room_floor == 9);
        const auto apart = make(shape, {4, {1, 1}, {1, 1}, std::numeric_limits<Index>::max(), 2, 8});
        verify(apart);
        CHECK(apart.rooms().size() == 1 && apart.measurements().overlap_rejections == 7);
        CHECK(apart.measurements().achievement == DungeonAchievement::attempt_limit);
        CHECK(apart.rooms()[0].lower[0] >= 2 && apart.rooms()[0].lower[1] >= 2);
        const auto margin = make({9, 9}, {1, {3, 3}, {3, 3}, 1, 3, 1});
        verify(margin);
        CHECK((margin.rooms()[0] == DungeonRoom{0, {3, 3}, {6, 6}}));
    }
    void replay() {
        CHECK(derive_stream_state(key) == 0xd2d031f03b516dab);
        auto placement = IntegerStream::for_stage({derive_stream_state(key), dungeon_placement_stage, 0});
        auto corridors = IntegerStream::for_stage({derive_stream_state(key), dungeon_corridor_stage, 0});
        for (auto value : {UINT64_C(0x5e4ccc6be9d72faf), UINT64_C(0xf44f3dd5b5b2b2ad), UINT64_C(0xdfb5850af0e83cb9)}) {
            CHECK(placement.next() == value);
        }
        for (auto value : {UINT64_C(0xd2cf01a41f51dafb), UINT64_C(0xa2182458a8aeb598), UINT64_C(0x9da1c107b17f2176)}) {
            CHECK(corridors.next() == value);
        }
        const auto dungeon = make(shape, small);
        verify(dungeon);
        auto unrelated = IntegerStream::for_stage({derive_stream_state(key), 0xdead, 0});
        for (unsigned i = 0; i < 37; ++i) {
            static_cast<void>(unrelated.next());
            static_cast<void>(corridors.next());
        }
        CHECK(same(dungeon, make(shape, small)));
        CHECK(same(dungeon, replay_dungeon(dungeon.descriptor()).value()));
        auto generous = dungeon.descriptor();
        generous.budgets = dungeon_hard_limits;
        CHECK(same(dungeon, replay_dungeon(generous).value()));
        CHECK(dungeon.effective() == small);
    }
    void ownership() {
        const auto original = make(shape, small), snapshot = original;
        auto copy = original;
        CHECK(copy.rooms().data() != original.rooms().data());
        CHECK(copy.connections().data() != original.connections().data());
        CHECK(copy.occupancy().data() != original.occupancy().data());
        copy = make(shape, large);
        CHECK(same(original, snapshot));
        auto rejected = original.descriptor();
        rejected.budgets.work = 0;
        error_is(replay_dungeon(rejected), FailureKind::budget_refused, Reason::work);
        rejected = original.descriptor();
        rejected.requested.minimum[0] = 0;
        error_is(replay_dungeon(rejected), FailureKind::invalid_input, Reason::dungeon_options);
        bool propagated = false;
        try {
            Deny deny;
            static_cast<void>(replay_dungeon(original.descriptor()));
        } catch (const std::bad_alloc&) {
            propagated = true;
        }
        CHECK(propagated && same(original, snapshot));
        const auto lifetime = [] {
            const auto domain = Domain::checked(shape).value();
            const DungeonReplay local{domain, key, small, budget};
            return replay_dungeon(local).value();
        }();
        CHECK(same(original, lifetime));
        Dungeon moved = std::move(copy);
        verify(moved);
    }
    void invalid() {
        const auto domain = Domain::checked(shape).value();
        DungeonReplay descriptor{domain, key, small, budget};
        descriptor.algorithm = static_cast<DungeonAlgorithm>(0);
        {
            Deny deny;
            error_is(replay_dungeon(descriptor), FailureKind::unsupported, Reason::dungeon_algorithm);
        }
        descriptor.algorithm = DungeonAlgorithm::rooms_and_chain;
        for (auto version : {0U, 2U}) {
            descriptor.version = version;
            Deny deny;
            error_is(replay_dungeon(descriptor), FailureKind::unsupported, Reason::dungeon_version);
        }
        for (unsigned which = 0; which < 5; ++which) {
            auto options = small;
            if (which == 0) {
                options.rooms = dungeon_max_rooms + 1;
            }
            if (which == 1) {
                options.attempts = dungeon_max_attempts + 1;
            }
            if (which == 2) {
                options.margin = 0;
            }
            if (which == 3) {
                options.minimum[1] = 0;
            }
            if (which == 4) {
                options.maximum[0] = 1;
            }
            Deny deny;
            error_is(dungeon_requirements(domain, options), FailureKind::invalid_input, Reason::dungeon_options);
        }
        const auto other = Domain::checked(std::array<Index, 3>{3, 3, 3}).value();
        {
            Deny deny;
            error_is(dungeon_requirements(other, small), FailureKind::invalid_input, Reason::dungeon_dimensions);
        }
        auto empty = small;
        empty.rooms = 0;
        const auto huge = Domain::checked(std::array<Index, 2>{std::numeric_limits<Index>::max() / 10 + 1, 1}).value();
        {
            Deny deny;
            error_is(dungeon_requirements(huge, empty), FailureKind::invalid_input, Reason::overflow);
        }
        {
            Deny deny;
            error_is(generate_dungeon({}, key, small, budget), FailureKind::invalid_input, Reason::dimensions);
        }
        {
            Deny deny;
            error_is(generate_dungeon(std::array<Index, 2>{0, 7}, key, small, budget), FailureKind::invalid_input, Reason::extent);
        }
        {
            Deny deny;
            error_is(
                generate_dungeon(std::array<Index, 2>{std::numeric_limits<Index>::max(), 2}, key, small, budget),
                FailureKind::invalid_input, Reason::overflow
            );
        }
    }
    void limits() {
        const auto d = Domain::checked(std::array<Index, 2>{3, 3}).value();
        DungeonConfiguration empty;
        empty.rooms = 0;
        const auto needed = dungeon_requirements(d, empty).value();
        CHECK(needed.cells == 9 && needed.storage_bytes == 4186 && needed.minimum_work == 36);
        const CaveBudgets exact{9, 4186, 36};
        const auto blank = generate_dungeon(std::array<Index, 2>{3, 3}, key, empty, exact).value();
        verify(blank);
        CHECK(blank.usage().work == 36);
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
            error_is(generate_dungeon(std::array<Index, 2>{3, 3}, key, empty, lower), FailureKind::budget_refused, reason);
        }
        const auto dungeon = make(shape, small), snapshot = dungeon;
        auto tight = dungeon.descriptor();
        tight.budgets = {tight.domain.cells(), dungeon_requirements(tight.domain, tight.requested)->storage_bytes, dungeon.usage().work};
        CHECK(same(dungeon, replay_dungeon(tight).value()));
        --tight.budgets.storage_bytes;
        {
            Deny deny;
            error_is(replay_dungeon(tight), FailureKind::budget_refused, Reason::storage);
        }
        ++tight.budgets.storage_bytes;
        --tight.budgets.work;
        error_is(replay_dungeon(tight), FailureKind::budget_refused, Reason::work);
        CHECK(same(dungeon, snapshot));
        const auto big = Domain::checked(std::array<Index, 2>{dungeon_hard_limits.cells + 1, 1}).value();
        {
            Deny deny;
            error_is(replay_dungeon({big, key, empty, dungeon_hard_limits}), FailureKind::budget_refused, Reason::cells);
        }
    }
    void exhaustion() {
        // First placement raw draw rejects for width bound2^63+1; the next is
        // accepted but oversized. Both rejection and discarded candidates count.
        const DungeonConfiguration oversized{1, {1, 1}, {(Index{1} << 63) + 1, 1}, 0, 1, 1};
        const auto result = make({5, 5}, oversized);
        verify(result);
        CHECK(result.measurements().fit_rejections == 1 && result.rooms().empty());
        CHECK(result.measurements().achievement == DungeonAchievement::attempt_limit);
        CHECK(result.usage().random_draws == 3); // rejected width, accepted width, height
        CHECK(result.usage().work == 104);       // 4*C + attempt + three raw draws
        auto short_work = result.descriptor();
        short_work.budgets.work = 103;
        error_is(replay_dungeon(short_work), FailureKind::budget_refused, Reason::work);
        // On a singleton interior, admission36 leaves9 work after init; even
        // one accepted room can exhaust final validation work, not placement proof.
        const auto one = make({3, 3}, {1, {1, 1}, {1, 1}, 0, 1, 1});
        auto short_one = one.descriptor();
        short_one.budgets.work = 36;
        error_is(replay_dungeon(short_one), FailureKind::budget_refused, Reason::work);
        CHECK(one.measurements().floor == 1);
    }
}
int main(int argc, char** argv) {
    struct Case {
        std::string_view name;
        void (*run)();
    };
    const std::array<Case, 9> cases{
        {{"dungeon_literal", literal},
         {"dungeon_layout", layout},
         {"dungeon_partial", partial},
         {"dungeon_controls", controls},
         {"dungeon_replay", replay},
         {"dungeon_ownership", ownership},
         {"dungeon_invalid", invalid},
         {"dungeon_limits", limits},
         {"dungeon_exhaustion", exhaustion}}
    };
    const std::string_view selection = argc == 2 ? argv[1] : "all";
    if (argc > 2) {
        std::cerr << "Expected one dungeon case name or all\n";
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
    std::cout << "Dungeon cases: " << passed << " passed, " << failed << " failed, 0 skipped; " << assertions << " assertions\n";
    return failed == 0 && passed != 0 ? 0 : 1;
}
