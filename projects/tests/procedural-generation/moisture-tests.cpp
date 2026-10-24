#include "moisture-checks.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

namespace MoistureAllocation {
    thread_local bool denied = false;
}
void* operator new(std::size_t n) {
    if (MoistureAllocation::denied)
        throw std::bad_alloc();
    if (void* p = std::malloc(n == 0 ? 1 : n))
        return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) {
    return ::operator new(n);
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
    namespace C = ProcGenMoistureChecks;
    constexpr auto limits = moisture_hard_limits;
    std::size_t assertions = 0;
    void check(bool b, const char* text, int line) {
        ++assertions;
        if (!b)
            throw std::runtime_error("line " + std::to_string(line) + ": " + text);
    }
#define CHECK(x) check(static_cast<bool>(x), #x, __LINE__)
    void near(double a, double b, double tolerance = 1e-10) {
        CHECK(std::abs(a - b) <= tolerance * std::max({1.0, std::abs(a), std::abs(b)}));
    }
    struct Deny {
        bool previous = MoistureAllocation::denied;
        Deny() { MoistureAllocation::denied = true; }
        ~Deny() { MoistureAllocation::denied = previous; }
    };
    template <class T> void error_is(const MoistureResult<T>& r, FailureKind k, MoistureReason reason) {
        CHECK(!r);
        CHECK(r.error().kind == k);
        CHECK(r.error().reason == reason);
    }
    ClimateBaseline flat(Index w = 3, Index h = 1, double dx = 1, double dy = 1, Index seconds = 1, Index tick = 0) {
        return C::baseline(
            w, h, std::vector<Height>(static_cast<std::size_t>(w * h)), std::vector<ClimateSurface>(static_cast<std::size_t>(w * h)),
            C::context(dx, dy, seconds), tick
        );
    }
    std::vector<MoistureCell> initial(const ClimateBaseline& b) {
        return std::vector<MoistureCell>(b.cells().size());
    }
    C::Winds winds(const ClimateBaseline& b, double u = 0, double v = 0, MoistureBoundary boundary = MoistureBoundary::closed) {
        return C::uniform_winds(b.descriptor().domain, b.descriptor().context.frame, u, v, boundary);
    }
    MoistureState start(const ClimateBaseline& b, std::span<const MoistureCell> a, MoistureModel model = C::passive_model()) {
        auto r = start_moisture(b, a, model, limits).value();
        const auto req = moisture_requirements(b.descriptor().domain).value();
        CHECK(r.tick() == b.descriptor().tick);
        CHECK(r.usage().work == req.start_work);
        CHECK(r.usage().storage_bytes == req.start_storage_bytes);
        return r;
    }
    MoistureAdvance advance(const MoistureState& s, const ClimateBaseline& b, const C::Winds& w, Index ticks = 1) {
        auto r = advance_moisture(s, b, w.view(), ticks, limits).value();
        C::inspect(s, r);
        const auto req = moisture_requirements(b.descriptor().domain).value();
        const auto n = r.state.usage().substeps_per_tick;
        const auto& d = b.descriptor().domain;
        const auto f = req.east_faces + req.north_faces,
                   faces = req.interior_faces + (w.boundary == MoistureBoundary::periodic
                                                     ? (d.extents()[0] > 1 ? d.extents()[1] : 0) + (d.extents()[1] > 1 ? d.extents()[0] : 0)
                                                     : 0);
        CHECK(
            r.state.usage().work ==
            64 + 9 * req.cells + 2 * f + n * (4 * req.cells + 1) + ticks + ticks * n * (7 * req.cells + 2 * faces + 1)
        );
        CHECK(r.state.usage().storage_bytes == req.advance_storage_bytes);
        CHECK(r.state.usage().substeps == n * ticks);
        return r;
    }
    bool same(const MoistureState& a, const MoistureState& b) {
        return a.tick() == b.tick() && a.model() == b.model() && a.baseline().descriptor() == b.baseline().descriptor() &&
               std::ranges::equal(a.baseline().cells(), b.baseline().cells()) && std::ranges::equal(a.cells(), b.cells()) &&
               std::ranges::equal(a.east_faces(), b.east_faces()) && std::ranges::equal(a.north_faces(), b.north_faces()) &&
               a.wind_interval() == b.wind_interval();
    }
    void transport_case() {
        auto b = flat(2, 2);
        auto a = initial(b);
        a[0].vapour_mm = 4;
        a[0].cloud_mm = 2;
        auto old = start(b, a);
        auto w = winds(b, .25, -.25, MoistureBoundary::periodic);
        auto step = advance(old, b, w);
        const std::array<double, 4> vapour{2, 1, 1, 0}, cloud{1, .5, .5, 0};
        for (std::size_t i = 0; i < 4; ++i) {
            near(step.state.cells()[i].vapour_mm, vapour[i]);
            near(step.state.cells()[i].cloud_mm, cloud[i]);
            CHECK(step.state.cells()[i].reference_kelvin == 280);
        }
        CHECK(step.state.usage().substeps_per_tick == 1);
        near(step.report.water.initial_m3, .006);
        near(step.report.water.final_m3, .006);
        b = flat();
        a = initial(b);
        a[0].vapour_mm = 1;
        a[1].vapour_mm = 2;
        a[2].vapour_mm = 4;
        old = start(b, a);
        w = winds(b, 1, 0, MoistureBoundary::periodic);
        step = advance(old, b, w);
        const std::array<double, 3> shifted{4, 1, 2};
        for (std::size_t i = 0; i < 3; ++i)
            near(step.state.cells()[i].vapour_mm, shifted[i]);
        CHECK(step.state.usage().substeps_per_tick == 1);
        b = flat(2, 2);
        a = initial(b);
        a[0].vapour_mm = 4;
        old = start(b, a);
        w = winds(b, 1, -1, MoistureBoundary::periodic);
        step = advance(old, b, w);
        const std::array<double, 4> split{2, 0, 0, 2};
        for (std::size_t i = 0; i < 4; ++i)
            near(step.state.cells()[i].vapour_mm, split[i]);
        CHECK(step.state.usage().substeps_per_tick == 2); // unsplit transport cannot translate diagonally in one step
    }
    C::Winds star(const ClimateBaseline& b, double speed, bool outward) {
        auto w = winds(b);
        const double s = outward ? speed : -speed;
        w.east[5] = -s;
        w.east[6] = s; // centre row faces x1/x2
        const double row_sign = b.descriptor().context.frame.north == RowNorth::increasing ? 1 : -1;
        w.north[4] = -s * row_sign;
        w.north[7] = s * row_sign;
        return w;
    }
    void faces_case() {
        auto b = flat(3, 3);
        auto a = initial(b);
        a[4].vapour_mm = 4;
        auto old = start(b, a);
        auto w = star(b, 1, true);
        auto step = advance(old, b, w);
        CHECK(step.state.usage().substeps_per_tick == 4);
        CHECK(step.state.usage().work == 694);
        near(step.state.cells()[4].vapour_mm, 0);
        for (std::size_t i : {1U, 3U, 5U, 7U})
            near(step.state.cells()[i].vapour_mm, 1);
        // Convergence: constant intensive T must remain constant, mass per area may increase.
        a = initial(b);
        for (auto i : {1U, 3U, 5U, 7U})
            a[i].vapour_mm = 1;
        old = start(b, a);
        w = star(b, 1, false);
        step = advance(old, b, w);
        CHECK(step.state.usage().substeps_per_tick == 4);
        const double retained = 81.0 / 256;
        for (auto i : {1U, 3U, 5U, 7U})
            near(step.state.cells()[i].vapour_mm, retained);
        near(step.state.cells()[4].vapour_mm, 4 * (1 - retained));
        for (const auto& cell : step.state.cells())
            CHECK(cell.reference_kelvin == 280);
        a[4].reference_kelvin = 500;
        a[1].reference_kelvin = 250;
        a[3].reference_kelvin = 260;
        a[5].reference_kelvin = 270;
        a[7].reference_kelvin = 280;
        old = start(b, a);
        step = advance(old, b, w);
        near(step.state.cells()[4].reference_kelvin, 265); // independent 4-way intensive mean
    }
    void phases_case() {
        auto b = flat(1, 1);
        auto a = initial(b);
        a[0] = {10, 2, 0, 280};
        auto m = C::passive_model();
        m.capacity_at_273_mm = 6;
        m.capacity_mm_per_kelvin = 0;
        m.condensation_per_second = .5;
        m.rain_per_second = .25;
        auto old = start(b, a, m);
        auto r = advance(old, b, winds(b));
        near(r.state.cells()[0].vapour_mm, 8);
        near(r.state.cells()[0].cloud_mm, 3);
        near(r.report.transfers[0].condensed_mm, 2);
        near(r.report.transfers[0].rain_mm, 1);
        near(r.report.water.initial_m3, .012);
        near(r.report.water.final_m3, .011);
        near(r.report.water.rain_export_m3, .001);
        a[0] = {2, 4, 0, 280};
        m.condensation_per_second = 0;
        m.rain_per_second = 0;
        m.cloud_evaporation_per_second = .5;
        old = start(b, a, m);
        r = advance(old, b, winds(b));
        near(r.state.cells()[0].vapour_mm, 4);
        near(r.state.cells()[0].cloud_mm, 2);
        near(r.report.transfers[0].cloud_evaporated_mm, 2);
        a[0] = {0, 0, 0, 280};
        m.condensation_per_second = 1;
        m.rain_per_second = 1;
        old = start(b, a, m);
        r = advance(old, b, winds(b), 10);
        near(r.report.water.rain_export_m3, 0);
        near(r.report.water.final_m3, 0);
        // Fractions saturate at1 instead of removing more cloud than is available.
        b = flat(1, 1, 1, 1, 10);
        a = initial(b);
        a[0].cloud_mm = 3;
        m.cloud_evaporation_per_second = 0;
        old = start(b, a, m);
        r = advance(old, b, winds(b));
        near(r.report.transfers[0].rain_mm, 3);
        near(r.state.cells()[0].cloud_mm, 0);
    }
    void sources_case() {
        const auto c = C::context(2, 3, 10);
        std::array<Height, 1> z{0};
        std::array<ClimateSurface, 1> surface{};
        surface[0].finite_water_fraction = .5;
        surface[0].replenished_water_fraction = .5;
        auto b = C::baseline(1, 1, z, surface, c);
        auto a = initial(b);
        a[0].finite_surface_mm = .25;
        auto model = C::passive_model();
        model.surface_evaporation_mm_per_second = .1;
        auto old = start(b, a, model);
        auto r = advance(old, b, winds(b), 2);
        near(r.state.cells()[0].finite_surface_mm, 0);
        near(r.state.cells()[0].vapour_mm, 1.25);
        near(r.report.transfers[0].finite_evaporation_mm, .25);
        near(r.report.transfers[0].replenished_mm, 1);
        near(r.report.water.initial_m3, .0015);
        near(r.report.water.replenished_m3, .006);
        near(r.report.water.final_m3, .0075);
        auto later = advance(r.state, b, winds(b));
        near(later.report.water.finite_evaporation_m3, 0);
        near(later.report.water.replenished_m3, .003);
        a[0].finite_surface_mm = 0;
        old = start(b, a, model);
        r = advance(old, b, winds(b));
        near(r.report.water.finite_evaporation_m3, 0);
    }
    MoistureAdvance ridge(unsigned kind, bool mirror = false) {
        std::vector<Height> z(21);
        std::vector<ClimateSurface> surface(21);
        std::vector<MoistureCell> a(21);
        for (std::size_t y = 0; y < 3; ++y) {
            if (kind != 0 && !(kind == 2 && y == 1))
                z[3 + y * 7] = 100;
            a[(mirror ? 5 : 1) + y * 7].vapour_mm = 9;
        }
        const auto b = C::baseline(7, 3, z, surface, C::context(1000, 1000, 1000));
        auto m = C::passive_model();
        m.condensation_per_second = 1;
        m.rain_per_second = 1;
        const auto old = start(b, a, m);
        return advance(old, b, winds(b, mirror ? -1 : 1), 4);
    }
    void ridge_case() {
        const auto flat_field = ridge(0), full = ridge(1), gap = ridge(2), mirrored = ridge(1, true);
        near(flat_field.report.water.rain_export_m3, 0);
        near(full.report.water.rain_export_m3, 8475);
        near(gap.report.water.rain_export_m3, 5650);
        for (std::size_t y = 0; y < 3; ++y) {
            near(full.report.transfers[3 + y * 7].rain_mm, 2.825);
            near(full.state.cells()[5 + y * 7].vapour_mm, 6.175);
            near(gap.state.cells()[5 + y * 7].vapour_mm, y == 1 ? 9 : 6.175);
        }
        CHECK(full.report.water.rain_export_m3 > gap.report.water.rain_export_m3);
        for (std::size_t i = 0; i < 21; ++i) {
            const auto reflected = (6 - i % 7) + (i / 7) * 7;
            near(full.state.cells()[i].vapour_mm, mirrored.state.cells()[reflected].vapour_mm);
            near(full.report.transfers[i].rain_mm, mirrored.report.transfers[reflected].rain_mm);
        }
    }
    void generated_case() {
        for (Index seed : {1U, 42U, 91U}) {
            const auto e =
                generate_elevation(std::array<Index, 2>{17, 11}, {seed, 0x67656f6772617068, 7}, {3, -80, 120}, elevation_hard_limits)
                    .value();
            auto c = C::context(2000, 2000, 1000);
            std::vector<ClimateSurface> surface(187);
            std::vector<MoistureCell> a(187);
            for (std::size_t i = 0; i < 187; ++i) {
                if (i % 17 == 0)
                    surface[i].replenished_water_fraction = 1;
                if (i % 17 == 1)
                    a[i].vapour_mm = 9;
            }
            surface[93].finite_water_fraction = 1;
            a[93].finite_surface_mm = .02;
            const auto b = derive_climate(e, surface, {1, 1}, c, 0, climate_hard_limits).value();
            auto m = C::passive_model();
            m.condensation_per_second = 1;
            m.rain_per_second = 1;
            m.surface_evaporation_mm_per_second = .00001;
            auto old = start(b, a, m);
            auto w = winds(b, 2);
            auto r = advance(old, b, w, 8);
            CHECK(r.state.baseline().descriptor().elevation.value() == e.descriptor());
            CHECK(r.report.water.rain_export_m3 > 0);
            near(r.report.water.finite_evaporation_m3, 80);
            near(r.report.water.replenished_m3, 3520);
            CHECK(r.state.tick() == 8);
            CHECK(same(r.state, advance(old, b, w, 8).state));
            CHECK(std::ranges::equal(old.baseline().cells(), b.cells()));
        }
    }
    void grouped_case() {
        const auto b = flat(3, 2, 2, 2, 2);
        auto a = initial(b);
        a[0] = {11, 3, 0, 290};
        a[2].reference_kelvin = 270;
        auto m = C::passive_model();
        m.condensation_per_second = .2;
        m.rain_per_second = .1;
        m.cloud_evaporation_per_second = .05;
        auto old = start(b, a, m);
        auto w = winds(b, .3, -.2, MoistureBoundary::periodic);
        auto direct = advance(old, b, w, 4), first = advance(old, b, w, 2), second = advance(first.state, b, w, 2);
        CHECK(std::ranges::equal(direct.state.cells(), second.state.cells()));
        CHECK(direct.state.tick() == second.state.tick());
        for (std::size_t i = 0; i < a.size(); ++i)
            near(direct.report.transfers[i].rain_mm, first.report.transfers[i].rain_mm + second.report.transfers[i].rain_mm);
        near(direct.report.water.rain_export_m3, first.report.water.rain_export_m3 + second.report.water.rain_export_m3);
        CHECK(first.state.wind_interval()->from_tick == 0);
        CHECK(second.state.wind_interval()->from_tick == 2);
        CHECK(direct.state.wind_interval()->ticks == 4);
        auto adequate = limits;
        adequate.substeps_per_tick = direct.state.usage().substeps_per_tick;
        adequate.work = direct.state.usage().work;
        CHECK(same(direct.state, advance_moisture(old, b, w.view(), 4, adequate).value().state));
    }
    void boundaries_case() {
        for (auto dimensions : {std::array<Index, 2>{1, 1}, std::array<Index, 2>{1, 3}, std::array<Index, 2>{3, 1}}) {
            const auto b = flat(dimensions[0], dimensions[1]);
            auto a = initial(b);
            a[0] = {2, 1, 0, 280};
            auto old = start(b, a);
            auto r = advance(old, b, winds(b));
            CHECK(std::ranges::equal(old.cells(), r.state.cells()));
            near(r.report.water.initial_m3, .003);
            auto wrap = advance(old, b, winds(b, dimensions[0] == 1 ? 100 : 0, dimensions[1] == 1 ? 100 : 0, MoistureBoundary::periodic));
            CHECK(std::ranges::equal(old.cells(), wrap.state.cells()));
            CHECK(wrap.state.usage().substeps_per_tick == 1);
        }
        for (auto north : {RowNorth::increasing, RowNorth::decreasing}) {
            auto c = C::context(1, 2, 2);
            c.frame.north = north;
            const auto b = C::baseline(1, 2, std::array<Height, 2>{0, 0}, std::array<ClimateSurface, 2>{}, c);
            auto a = initial(b);
            a[north == RowNorth::increasing ? 0 : 1].vapour_mm = 1;
            auto old = start(b, a);
            auto r = advance(old, b, winds(b, 0, 1));
            near(r.state.cells()[north == RowNorth::increasing ? 1 : 0].vapour_mm, 1);
            near(r.report.water.final_m3, .002);
        }
    }
    void capacity_case() {
        MoistureModel m;
        near(moisture_capacity(273.15, m).value(), 6);
        near(moisture_capacity(263.15, m).value(), 1);
        near(moisture_capacity(293.15, m).value(), 16);
        near(moisture_capacity(1, m).value(), 0);
        near(moisture_capacity(2000, m).value(), 100);
        m.minimum_capacity_mm = 2;
        near(moisture_capacity(1, m).value(), 2);
        m.maximum_capacity_mm = 1;
        error_is(moisture_capacity(280, m), FailureKind::invalid_input, MoistureReason::model);
        m = {};
        m.version = 2;
        error_is(moisture_capacity(280, m), FailureKind::unsupported, MoistureReason::version);
        m = {};
        m.numerics_version = 0;
        error_is(moisture_capacity(280, m), FailureKind::unsupported, MoistureReason::version);
        m = {};
        error_is(moisture_capacity(std::numeric_limits<double>::quiet_NaN(), m), FailureKind::invalid_input, MoistureReason::initial);
    }
    void invalid_case() {
        const auto b = flat(3, 3);
        auto a = initial(b);
        auto old = start(b, a);
        const auto snapshot = old;
        auto w = winds(b);
        error_is(
            start_moisture(b, std::span<const MoistureCell>(a).first(8), C::passive_model(), limits), FailureKind::invalid_input,
            MoistureReason::shape
        );
        error_is(
            advance_moisture(old, b, {std::span<const double>{}, w.north, MoistureBoundary::closed}, 1, limits), FailureKind::invalid_input,
            MoistureReason::shape
        );
        for (double bad : {-1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
            for (auto member :
                 {&MoistureCell::vapour_mm, &MoistureCell::cloud_mm, &MoistureCell::finite_surface_mm, &MoistureCell::reference_kelvin}) {
                a = initial(b);
                a[8].*member = bad;
                MoistureResult<MoistureState> r = std::unexpected(MoistureFailure{});
                {
                    Deny deny;
                    r = start_moisture(b, a, C::passive_model(), limits);
                }
                error_is(r, FailureKind::invalid_input, MoistureReason::initial);
                CHECK(r.error().work == 64 + 2 * 9);
            }
        }
        a = initial(b);
        a[0].finite_surface_mm = 1;
        error_is(start_moisture(b, a, C::passive_model(), limits), FailureKind::invalid_input, MoistureReason::initial);
        for (auto member :
             {&MoistureModel::capacity_at_273_mm, &MoistureModel::capacity_mm_per_kelvin, &MoistureModel::minimum_capacity_mm,
              &MoistureModel::maximum_capacity_mm, &MoistureModel::condensation_per_second, &MoistureModel::cloud_evaporation_per_second,
              &MoistureModel::rain_per_second, &MoistureModel::surface_evaporation_mm_per_second}) {
            auto m = C::passive_model();
            m.*member = std::numeric_limits<double>::quiet_NaN();
            error_is(start_moisture(b, initial(b), m, limits), FailureKind::invalid_input, MoistureReason::model);
        }
        for (double bad : {1001.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
            w = winds(b);
            w.east[5] = bad;
            error_is(advance_moisture(old, b, w.view(), 1, limits), FailureKind::invalid_input, MoistureReason::wind);
        }
        w = winds(b);
        w.east[0] = 1;
        error_is(advance_moisture(old, b, w.view(), 1, limits), FailureKind::invalid_input, MoistureReason::boundary);
        w = winds(b, 1, 0, MoistureBoundary::periodic);
        w.east[0] = 0;
        error_is(advance_moisture(old, b, w.view(), 1, limits), FailureKind::invalid_input, MoistureReason::boundary);
        w = winds(b);
        w.boundary = static_cast<MoistureBoundary>(3);
        error_is(advance_moisture(old, b, w.view(), 1, limits), FailureKind::unsupported, MoistureReason::boundary);
        const auto c = C::context();
        auto c2 = c;
        c2.frame.datum_metres = 1;
        auto different = C::baseline(3, 3, std::vector<Height>(9), std::vector<ClimateSurface>(9), c2);
        error_is(advance_moisture(old, different, winds(b).view(), 1, limits), FailureKind::invalid_input, MoistureReason::context);
        c2 = c;
        c2.expected_revision = {2, 2};
        different = C::baseline(3, 3, std::vector<Height>(9), std::vector<ClimateSurface>(9), c2);
        error_is(advance_moisture(old, different, winds(b).view(), 1, limits), FailureKind::invalid_input, MoistureReason::context);
        auto z = std::vector<Height>(9);
        z[8] = 1;
        different = C::baseline(3, 3, z, std::vector<ClimateSurface>(9), c);
        error_is(advance_moisture(old, different, winds(b).view(), 1, limits), FailureKind::invalid_input, MoistureReason::context);
        CHECK(same(old, snapshot));
        const auto single = Domain::checked(std::array<Index, 1>{1}).value();
        error_is(moisture_requirements(single), FailureKind::invalid_input, MoistureReason::dimensions);
        const auto huge = Domain::checked(std::array<Index, 2>{std::numeric_limits<Index>::max(), 1}).value();
        {
            Deny deny;
            error_is(moisture_requirements(huge), FailureKind::invalid_input, MoistureReason::overflow);
        }
    }
    void limits_case() {
        const auto b = flat(1, 1);
        auto a = initial(b);
        const auto req = moisture_requirements(b.descriptor().domain).value();
        CHECK(req.start_work == 72);
        CHECK(req.start_storage_bytes == 16600);
        CHECK(req.advance_storage_bytes == 16720);
        auto budget = limits;
        budget.cells = 1;
        budget.storage_bytes = req.start_storage_bytes;
        budget.work = req.start_work;
        auto old = start_moisture(b, a, C::passive_model(), budget).value();
        const auto held = old;
        auto w = winds(b);
        for (unsigned which = 0; which < 3; ++which) {
            auto short_budget = budget;
            if (which == 0)
                --short_budget.cells;
            if (which == 1)
                --short_budget.storage_bytes;
            if (which == 2)
                --short_budget.work;
            MoistureResult<MoistureState> r = std::unexpected(MoistureFailure{});
            {
                Deny deny;
                r = start_moisture(b, a, C::passive_model(), short_budget);
            }
            error_is(
                r, FailureKind::budget_refused,
                which == 0   ? MoistureReason::cells
                : which == 1 ? MoistureReason::storage
                             : MoistureReason::work
            );
            CHECK(r.error().work == 0);
        }
        auto r = advance(old, b, w, 2);
        CHECK(r.state.usage().work == 104);
        budget = limits;
        budget.cells = 1;
        budget.storage_bytes = req.advance_storage_bytes;
        budget.work = 104;
        budget.ticks = 2;
        budget.substeps_per_tick = 1;
        CHECK(advance_moisture(old, b, w.view(), 2, budget));
        for (unsigned which = 0; which < 5; ++which) {
            auto short_budget = budget;
            if (which == 0)
                --short_budget.cells;
            if (which == 1)
                --short_budget.storage_bytes;
            if (which == 2)
                --short_budget.work;
            if (which == 3)
                --short_budget.ticks;
            if (which == 4)
                --short_budget.substeps_per_tick;
            MoistureResult<MoistureAdvance> refused = std::unexpected(MoistureFailure{});
            {
                Deny deny;
                refused = advance_moisture(old, b, w.view(), 2, short_budget);
            }
            const auto reason = which == 0   ? MoistureReason::cells
                                : which == 1 ? MoistureReason::storage
                                : which == 2 ? MoistureReason::work
                                : which == 3 ? MoistureReason::ticks
                                             : MoistureReason::substeps;
            error_is(refused, FailureKind::budget_refused, reason);
            CHECK(same(old, held));
        }
        error_is(advance_moisture(old, b, w.view(), 0, limits), FailureKind::invalid_input, MoistureReason::ticks);
        error_is(advance_moisture(old, b, w.view(), 11, limits), FailureKind::budget_refused, MoistureReason::ticks);
        const auto last = flat(1, 1, 1, 1, 1, std::numeric_limits<Index>::max());
        const auto terminal = start(last, initial(last));
        error_is(advance_moisture(terminal, last, w.view(), 1, limits), FailureKind::invalid_input, MoistureReason::overflow);
        const auto nine = flat(3, 3);
        auto q = initial(nine);
        q[4].vapour_mm = 4;
        const auto source = start(nine, q);
        auto high = star(nine, 1, true);
        auto exact = advance(source, nine, high);
        budget = limits;
        budget.work = exact.state.usage().work;
        budget.substeps_per_tick = 4;
        CHECK(advance_moisture(source, nine, high.view(), 1, budget));
        --budget.work;
        auto refused = advance_moisture(source, nine, high.view(), 1, budget);
        error_is(refused, FailureKind::budget_refused, MoistureReason::work);
        CHECK(refused.error().work > 0);
        budget = limits;
        budget.substeps_per_tick = 3;
        error_is(advance_moisture(source, nine, high.view(), 1, budget), FailureKind::budget_refused, MoistureReason::substeps);
        high = star(nine, 16, true);
        budget = limits;
        budget.work = 319; // minimum one-step admission, but rejected CFL trials exhaust at runtime
        refused = advance_moisture(source, nine, high.view(), 1, budget);
        error_is(refused, FailureKind::budget_refused, MoistureReason::work);
        CHECK(refused.error().work > 0 && refused.error().work <= 319);
        exact = advance(source, nine, high);
        CHECK(exact.state.usage().substeps_per_tick == 64);
        high = star(nine, 17, true);
        error_is(advance_moisture(source, nine, high.view(), 1, limits), FailureKind::budget_refused, MoistureReason::substeps);
        q = initial(nine);
        for (auto i : {1U, 3U, 5U, 7U})
            q[i].vapour_mm = 1e12;
        const auto concentrated = start(nine, q);
        high = star(nine, 1, false);
        error_is(advance_moisture(concentrated, nine, high.view(), 1, limits), FailureKind::unsupported, MoistureReason::numeric);
        const auto full = flat(128, 128);
        const auto accepted = start(full, initial(full));
        const auto maximum = advance(accepted, full, winds(full));
        CHECK(maximum.state.cells().size() == 16384);
        CHECK(maximum.state.usage().storage_bytes == 5261312);
        CHECK(maximum.state.usage().work < 100'000'000);
        bool thrown = false;
        {
            Deny deny;
            try {
                static_cast<void>(start_moisture(b, a, C::passive_model(), limits));
            } catch (const std::bad_alloc&) {
                thrown = true;
            }
        }
        CHECK(thrown);
        CHECK(same(old, held));
    }
    void ownership_case() {
        const auto complete = [] {
            const auto b = flat(3, 1);
            auto a = initial(b);
            a[0].vapour_mm = 1;
            auto s = start(b, a);
            auto w = winds(b, .5, 0, MoistureBoundary::periodic);
            return advance(s, b, w);
        }();
        auto copied = complete;
        CHECK(same(complete.state, copied.state));
        CHECK(complete.state.cells().data() != copied.state.cells().data());
        CHECK(complete.report.transfers.data() != copied.report.transfers.data());
        const auto before = complete.report.transfers;
        const auto b = flat();
        auto a = initial(b);
        a[0].vapour_mm = 2;
        auto s = start(b, a);
        auto w = winds(b, .5);
        auto step = advance(s, b, w);
        CHECK(step.state.baseline().cells().data() != s.baseline().cells().data());
        CHECK(step.state.east_faces().data() != w.east.data());
        w.east[1] = 0;
        CHECK(step.state.east_faces()[1] == .5);
        auto bad = w;
        bad.east[0] = 1;
        error_is(advance_moisture(step.state, b, bad.view(), 1, limits), FailureKind::invalid_input, MoistureReason::boundary);
        CHECK(std::ranges::equal(complete.report.transfers, before));
        CHECK(same(complete.state, copied.state));
        auto moved = std::move(copied);
        CHECK(same(complete.state, moved.state));
        // Same relevant baseline with different sufficient admission metadata is compatible.
        auto c = C::context();
        auto z = std::vector<Height>(3);
        auto sf = std::vector<ClimateSurface>(3);
        auto smaller = climate_hard_limits;
        smaller.work = 160;
        const auto equivalent = derive_climate({b.descriptor().domain, z, sf, {1, 1}}, c, 0, smaller).value();
        CHECK(advance_moisture(s, equivalent, winds(b).view(), 1, limits));
    }
}
int main(int argc, char** argv) {
    try {
        if (argc != 2)
            throw std::runtime_error("one moisture case required");
        const std::string n = argv[1];
        if (n == "moisture_transport")
            transport_case();
        else if (n == "moisture_faces")
            faces_case();
        else if (n == "moisture_phases")
            phases_case();
        else if (n == "moisture_sources")
            sources_case();
        else if (n == "moisture_ridge")
            ridge_case();
        else if (n == "moisture_generated")
            generated_case();
        else if (n == "moisture_grouped")
            grouped_case();
        else if (n == "moisture_boundaries")
            boundaries_case();
        else if (n == "moisture_capacity")
            capacity_case();
        else if (n == "moisture_invalid")
            invalid_case();
        else if (n == "moisture_limits")
            limits_case();
        else if (n == "moisture_ownership")
            ownership_case();
        else
            throw std::runtime_error("unknown moisture case");
        std::cout << "PASS " << n << ": " << assertions << " assertions\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL moisture: " << e.what() << '\n';
        return 1;
    }
}
