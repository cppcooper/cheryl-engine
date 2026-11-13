#include "settlement-sites.h"

#include <algorithm>
#include <iostream>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace {
    using namespace ProcGen;
    void check(bool ok) { if (!ok) { throw std::runtime_error("settlement assertion failed"); } }
    Domain domain(Index w, Index h) {
        const std::array<Index, 2> extents{w, h};
        return Domain::checked(extents).value();
    }
    void expect(const SettlementSites& result, std::initializer_list<SettlementSite> sites, Index requested) {
        check(result.requested() == requested && result.achieved() == sites.size());
        check(result.shortfall() == requested - sites.size());
        check(std::equal(result.sites().begin(), result.sites().end(), sites.begin(), sites.end()));
    }
    void literal() {
        const auto d = domain(5, 4);
        const std::array<Height, 20> flat{10,10,10,10,10, 10,10,10,10,10, 10,10,10,10,10, 10,10,10,10,10};
        const auto before = flat;
        const auto a = select_settlement_sites(d, flat, {4,0,0,1}, settlement_hard_limits).value();
        expect(a, {{6,1,1,0,10},{7,2,1,0,10},{8,3,1,0,10},{11,1,2,0,10}}, 4);
        const auto b = select_settlement_sites(d, flat, {4,0,0,1}, settlement_hard_limits).value();
        check(std::equal(a.sites().begin(), a.sites().end(), b.sites().begin(), b.sites().end()));
        check(a.work() == 160 && flat == before);
        expect(select_settlement_sites(d, flat, {4,10,0,1}, settlement_hard_limits).value(), {}, 4);
        expect(select_settlement_sites(d, flat, {4,11,0,1}, settlement_hard_limits).value(), {}, 4);
        // Non-square field; independent cardinal slopes at interior indexes6,7,8:
        // max(1,2,1,1)=2; max(2,2,10,3)=10; max(2,1,1,1)=2.
        const std::array<Height, 15> asymmetric{10,9,22,9,10, 9,10,12,10,9, 10,9,9,9,10};
        expect(select_settlement_sites(domain(5,3), asymmetric, {3,0,10,1}, settlement_hard_limits).value(),
               {{6,1,1,2,10},{8,3,1,2,10},{7,2,1,10,12}}, 3);
        expect(select_settlement_sites(domain(5,3), asymmetric, {3,0,2,1}, settlement_hard_limits).value(),
               {{6,1,1,2,10},{8,3,1,2,10}}, 3);
    }
    void spacing() {
        const std::vector<Height> flat(25, 7);
        expect(select_settlement_sites(domain(5,5), flat, {5,0,0,3}, settlement_hard_limits).value(),
               {{6,1,1,0,7},{13,3,2,0,7}}, 5);
        expect(select_settlement_sites(domain(5,5), flat, {3,0,0,UINT64_MAX}, settlement_hard_limits).value(),
               {{6,1,1,0,7}}, 3);
        expect(select_settlement_sites(domain(5,5), flat, {3,0,0,2}, settlement_hard_limits).value(),
               {{6,1,1,0,7},{8,3,1,0,7},{12,2,2,0,7}}, 3);
        const std::vector<Height> large(49, 7);
        check(select_settlement_sites(domain(7,7), large, {16,0,0,1}, settlement_hard_limits)->achieved() == 16);
    }
    void tiny() {
        for (Index w = 1; w <= 3; ++w) {
            for (Index h = 1; h <= 3; ++h) {
                const std::vector<Height> heights(static_cast<std::size_t>(w*h), 1);
                const auto result = select_settlement_sites(domain(w,h), heights, {2,0,0,1}, settlement_hard_limits).value();
                if (w == 3 && h == 3) { expect(result, {{4,1,1,0,1}}, 2); }
                else { expect(result, {}, 2); check(result.work() == 2); }
            }
        }
    }
    void extremes() {
        constexpr auto lo = std::numeric_limits<Height>::min();
        constexpr auto hi = std::numeric_limits<Height>::max();
        const std::array<Height,9> heights{lo,lo,lo, lo,hi,lo, lo,lo,lo};
        expect(select_settlement_sites(domain(3,3), heights, {1,lo,UINT32_MAX,1}, settlement_hard_limits).value(),
               {{4,1,1,UINT32_MAX,hi}}, 1);
        expect(select_settlement_sites(domain(3,3), heights, {1,lo,UINT32_MAX-1,1}, settlement_hard_limits).value(), {}, 1);
        const std::array<Height,9> negative{hi,hi,hi, hi,lo,hi, hi,hi,hi};
        expect(select_settlement_sites(domain(3,3), negative, {1,lo,UINT32_MAX,1}, settlement_hard_limits).value(), {}, 1);
        const std::array<Height,9> reverse{hi,hi,hi, hi,lo+1,hi, hi,hi,hi};
        expect(select_settlement_sites(domain(3,3), reverse, {1,lo,UINT32_MAX,1}, settlement_hard_limits).value(),
               {{4,1,1,UINT32_MAX-1,lo+1}}, 1);
    }
    void invalid() {
        const auto d = domain(3,3);
        const std::array<Height,9> heights{};
        for (const auto& options : {SettlementOptions{17,0,0,1}, SettlementOptions{1,0,0,0}, SettlementOptions{1,0,UINT64_MAX,1}, SettlementOptions{0,0,0,0}}) {
            const auto result = select_settlement_sites(d, heights, options, settlement_hard_limits);
            check(!result && result.error() == SettlementFailure{FailureKind::invalid_input, SettlementReason::options});
        }
        const std::array<Index,1> extents{9};
        check(!settlement_requirements(Domain::checked(extents).value(), {}));
        check(!select_settlement_sites(d, std::span<const Height>{}, {}, settlement_hard_limits));
        const std::array<Index,2> huge{UINT64_MAX/3,3};
        const auto overflow = settlement_requirements(Domain::checked(huge).value(), {16,0,0,1});
        check(!overflow && overflow.error().reason == SettlementReason::overflow);
    }
    void limits() {
        const auto d = domain(5,4);
        const std::vector<Height> heights(20, 10);
        const auto req = settlement_requirements(d, {4,0,0,1}).value();
        check(req.cells == 20 && req.interior_cells == 6 && req.work == 160);
        check(req.storage_bytes == sizeof(SettlementSites) + sizeof(SettlementSite));
        const CaveBudgets exact{20,req.storage_bytes,160};
        const auto good = select_settlement_sites(d, heights, {4,0,0,1}, exact).value();
        check(good.work() == 160 && good.storage_bytes() == req.storage_bytes && good.achieved() == 4);
        for (const auto& pair : {std::pair{CaveBudgets{19,req.storage_bytes,160},SettlementReason::cells},
                                std::pair{CaveBudgets{20,req.storage_bytes-1,160},SettlementReason::storage},
                                std::pair{CaveBudgets{20,req.storage_bytes,159},SettlementReason::work}}) {
            const auto refused = select_settlement_sites(d, heights, {4,0,0,1}, pair.first);
            check(!refused && refused.error() == SettlementFailure{FailureKind::budget_refused,pair.second});
        }
        // Water and spacing exhaustion have the SAME admitted work, not a partial budget success.
        const auto water = select_settlement_sites(d, heights, {4,10,0,1}, exact).value();
        check(water.work() == 160 && water.achieved() == 0);
        const auto spaced = select_settlement_sites(d, heights, {4,0,0,100}, exact).value();
        check(spaced.work() == 160 && spaced.achieved() == 1);
        const auto zero = select_settlement_sites(d, heights, {0,0,0,1}, {20,req.storage_bytes,0}).value();
        expect(zero, {}, 0); check(zero.work() == 0);
        for (const auto& budget : {CaveBudgets{0,req.storage_bytes,160}, CaveBudgets{20,0,160}, CaveBudgets{20,req.storage_bytes,0}}) {
            check(!select_settlement_sites(d, heights, {4,0,0,1}, budget));
        }
        const auto big = domain(257,256);
        const std::vector<Height> many(static_cast<std::size_t>(big.cells()),1);
        check(!select_settlement_sites(big, many, {0,0,0,1}, {UINT64_MAX,UINT64_MAX,UINT64_MAX}));
    }
    SettlementSites detached() {
        const std::array<Index,2> extents{5,4};
        const auto field = generate_elevation(extents, {42,7,1}, {3,10,10}, elevation_hard_limits).value();
        const auto original = std::vector<Height>(field.heights().begin(),field.heights().end());
        const auto result = select_settlement_sites(field, {2,0,0,2}, settlement_hard_limits).value();
        check(std::equal(original.begin(),original.end(),field.heights().begin(),field.heights().end()));
        return result;
    }
    void ownership() {
        const auto a = detached();
        expect(a, {{6,1,1,0,10},{8,3,1,0,10}}, 2);
        const auto b = a;
        auto c = b;
        c = detached();
        expect(c, {{6,1,1,0,10},{8,3,1,0,10}}, 2);
        check(a.options() == b.options() && a.work() == b.work());
        const auto moved = std::move(c);
        expect(moved, {{6,1,1,0,10},{8,3,1,0,10}}, 2);
        std::vector<Height> mutable_input(9, 20);
        const auto saved = select_settlement_sites(domain(3,3), mutable_input, {1,0,0,1}, settlement_hard_limits).value();
        std::fill(mutable_input.begin(), mutable_input.end(), -100);
        expect(saved, {{4,1,1,0,20}}, 1);
    }
}
int main(int argc, char** argv) {
    try {
        if (argc != 2) { throw std::runtime_error("one settlement case required"); }
        const std::string_view name = argv[1];
        if (name == "settlement_literal") { literal(); }
        else if (name == "settlement_spacing") { spacing(); }
        else if (name == "settlement_tiny") { tiny(); }
        else if (name == "settlement_extremes") { extremes(); }
        else if (name == "settlement_invalid") { invalid(); }
        else if (name == "settlement_limits") { limits(); }
        else if (name == "settlement_ownership") { ownership(); }
        else { throw std::runtime_error("unknown settlement case"); }
        std::cout << name << ": passed, 0 skipped\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "settlement failure: " << error.what() << '\n';
        return 1;
    }
}
