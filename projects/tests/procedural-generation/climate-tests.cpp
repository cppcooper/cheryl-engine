#include "climate-checks.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <numbers>
#include <stdexcept>
#include <string>
#include <utility>

namespace ClimateAllocation {
    thread_local bool denied = false;
}
void* operator new(std::size_t size) {
    if (ClimateAllocation::denied)
        throw std::bad_alloc();
    if (void* memory = std::malloc(size == 0 ? 1 : size))
        return memory;
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
    ClimateContext example_context() {
        ClimateContext c;
        c.expected_revision = {1, 1};
        c.frame = {2000, 2000, 10, 0, 40, 0, RowNorth::decreasing};
        c.calendar = {86'400, 360, 3600, 0, 23.5};
        return c;
    }

    constexpr auto budget = climate_hard_limits;
    constexpr double pi = std::numbers::pi;
    std::size_t assertions = 0;
    void check(bool ok, const char* expression, int line) {
        ++assertions;
        if (!ok)
            throw std::runtime_error("line " + std::to_string(line) + ": " + expression);
    }
#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)
    void near(double a, double b, double tolerance = 1e-10) {
        CHECK(std::abs(a - b) <= tolerance * std::max({1.0, std::abs(a), std::abs(b)}));
    }
    struct Deny {
        bool previous = ClimateAllocation::denied;
        Deny() { ClimateAllocation::denied = true; }
        ~Deny() { ClimateAllocation::denied = previous; }
    };
    template <class T> void error_is(const ClimateResult<T>& r, FailureKind kind, ClimateReason reason) {
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
        std::vector<ClimateSurface> surface;
        ClimateInput input() const { return {d, heights, surface, {1, 1}}; }
    };
    Literal fixture(Index w = 3, Index h = 3) {
        return {
            domain(w, h), std::vector<Height>(static_cast<std::size_t>(w * h), 0),
            std::vector<ClimateSurface>(static_cast<std::size_t>(w * h))
        };
    }
    ClimateBaseline make(const Literal& f, const ClimateContext& c = example_context(), Index tick = 12) {
        auto r = derive_climate(f.input(), c, tick, budget).value();
        ProcGenClimateChecks::inspect(f.input(), r);
        auto req = climate_requirements(f.d).value();
        CHECK(r.usage().work == req.work);
        CHECK(r.usage().storage_bytes == req.storage_bytes);
        return r;
    }
    bool same(const ClimateBaseline& a, const ClimateBaseline& b) {
        return a.descriptor() == b.descriptor() && std::ranges::equal(a.cells(), b.cells()) && a.measurements() == b.measurements();
    }
    SolarCalendar solar_calendar() {
        return {86'400, 360, 1, 0, 30};
    }
    void sun_case() {
        auto c = solar_calendar();
        auto equinox = sample_sun(c, 0, 0, 180).value(); // Greenwich midnight, longitude180 local noon, phase0
        near(equinox.east, 0);
        near(equinox.north, 0);
        near(equinox.up, 1);
        near(equinox.daily_mean_cosine, 1 / pi);
        near(equinox.daylight_fraction, .5);
        auto night = sample_sun(c, 0, 0, 0).value();
        near(night.up, -1);
        auto sunrise = sample_sun(c, 0, 0, 90).value();
        near(sunrise.east, 1);
        CHECK(sunrise.up == 0);
        auto sunset = sample_sun(c, 0, 0, -90).value();
        near(sunset.east, -1);
        CHECK(sunset.up == 0);
        auto forty_five = sample_sun(c, 0, 45, 180).value();
        near(forty_five.north, -std::sqrt(.5));
        near(forty_five.up, std::sqrt(.5));
        near(forty_five.daily_mean_cosine, std::sqrt(.5) / pi);
        auto summer = sample_sun(c, 90 * 86'400, 30, 180).value();
        near(summer.up, 1);
        near(summer.north, 0);
        const double a = .25, b = .75, h = std::acos(-1.0 / 3);
        near(summer.daily_mean_cosine, (a * h + b * std::sin(h)) / pi);
        near(summer.daylight_fraction, h / pi);
        // Independent numerical integral of positive horizontal cosine at declination30/latitude30.
        double integral = 0;
        for (unsigned k = 0; k < 20'000; ++k)
            integral += std::max(0.0, .25 + .75 * std::cos(2 * pi * (static_cast<double>(k) + .5) / 20'000));
        near(summer.daily_mean_cosine, integral / 20'000, 1e-8);
        for (double latitude : {-90.0, 90.0}) {
            auto p = sample_sun(c, 90 * 86'400, latitude, 0).value();
            near(p.up, latitude > 0 ? .5 : -.5);
            near(p.daylight_fraction, latitude > 0 ? 1 : 0);
            near(p.daily_mean_cosine, latitude > 0 ? .5 : 0);
        }
        for (double latitude : {-90.0, 90.0}) {
            auto p = sample_sun(c, 180 * 86'400, latitude, 0).value();
            CHECK(p.up == 0);
            CHECK(p.daily_mean_cosine == 0);
            CHECK(p.daylight_fraction == 0);
        }
        c.tilt_degrees = 0;
        for (double latitude : {-90.0, 90.0}) {
            auto p = sample_sun(c, 0, latitude, 0).value();
            CHECK(p.up == 0);
            CHECK(p.daily_mean_cosine == 0);
            CHECK(p.daylight_fraction == 0);
        }
    }
    void seasons_case() {
        auto c = solar_calendar();
        for (double latitude : {-50.0, 0.0, 50.0}) {
            const auto summer = sample_sun(c, 90 * 86'400, latitude, 180).value();
            const auto winter = sample_sun(c, 270 * 86'400, latitude, 180).value();
            near(summer.up, std::cos((latitude - 30) * pi / 180));
            near(winter.up, std::cos((latitude + 30) * pi / 180));
            if (latitude > 0)
                CHECK(summer.daily_mean_cosine > winter.daily_mean_cosine);
            if (latitude < 0)
                CHECK(summer.daily_mean_cosine < winter.daily_mean_cosine);
            if (latitude == 0)
                near(summer.daily_mean_cosine, winter.daily_mean_cosine);
        }
        c.tilt_degrees = 0;
        for (Index day : {0U, 90U, 180U, 270U}) {
            const auto p = sample_sun(c, day * 86'400, 50, 180).value();
            near(p.up, std::cos(50 * pi / 180));
            near(p.daily_mean_cosine, std::cos(50 * pi / 180) / pi);
        }
    }
    void calendar_case() {
        auto c = solar_calendar();
        const Index year = c.day_seconds * c.year_days;
        const Index last = std::numeric_limits<Index>::max();
        for (auto epoch :
             {std::numeric_limits<std::int64_t>::min(), std::int64_t{-1}, std::int64_t{0}, std::numeric_limits<std::int64_t>::max()}) {
            c.epoch_seconds = epoch;
            for (Index step : {Index{1}, Index{3600}, Index{1'000'000}}) {
                c.seconds_per_tick = step;
                CHECK(sample_sun(c, last, 25, -30).value() == sample_sun(c, last % year, 25, -30).value());
                CHECK(sample_sun(c, year, 25, -30).value() == sample_sun(c, 0, 25, -30).value());
            }
        }
        c = solar_calendar();
        c.epoch_seconds = -1;
        auto other = c;
        other.epoch_seconds = static_cast<std::int64_t>(year) - 1;
        CHECK(sample_sun(c, 0, 25, 0).value() == sample_sun(other, 0, 25, 0).value());
        c.epoch_seconds = 0;
        CHECK(sample_sun(c, year, 25, 0).value() == sample_sun(c, 0, 25, 0).value());
        // Zero tilt isolates day cycle across the midnight boundary.
        c.tilt_degrees = 0;
        CHECK(sample_sun(c, 86'400, 0, 0).value() == sample_sun(c, 0, 0, 0).value());
    }
    void frame_case() {
        auto f = fixture();
        f.heights = {0, 10, 20, 30, 40, 50, 60, 70, 80};
        ClimateContext c = example_context();
        c.frame = {1000, 2000, 10, 100, 0, 0, RowNorth::decreasing};
        c.calendar.tilt_degrees = 0;
        auto a = make(f, c);
        for (std::size_t i = 0; i < 9; ++i) {
            near(a.cells()[i].z_metres, 100 + 10 * static_cast<double>(f.heights[i]));
            near(a.cells()[i].east_slope, .1);
            near(a.cells()[i].north_slope, -.15);
        }
        near(a.cells()[4].latitude_degrees, 0);
        near(a.cells()[4].longitude_degrees, 0);
        near(a.cells()[0].latitude_degrees, 2000 / 6'371'000.0 * 180 / pi);
        near(a.cells()[0].longitude_degrees, -1000 / 6'371'000.0 * 180 / pi);
        CHECK(a.cells()[0].longitude_degrees < a.cells()[1].longitude_degrees);
        CHECK(a.cells()[0].latitude_degrees > a.cells()[3].latitude_degrees);
        c.frame.north = RowNorth::increasing;
        auto b = make(f, c);
        near(b.cells()[0].north_slope, .15);
        near(b.cells()[0].latitude_degrees, -a.cells()[0].latitude_degrees);
        const auto before = f.heights;
        for (auto& v : f.heights)
            v += 10;
        auto raised = make(f, c);
        near(raised.cells()[4].air_reference_kelvin, b.cells()[4].air_reference_kelvin - .65);
        near(raised.cells()[4].seasonal_reference_kelvin, b.cells()[4].seasonal_reference_kelvin);
        c.frame.datum_metres = 0;
        auto compensated = make(f, c);
        near(compensated.cells()[4].z_metres, b.cells()[4].z_metres);
        near(compensated.cells()[4].air_reference_kelvin, b.cells()[4].air_reference_kelvin);
        CHECK(f.heights != before); // original source remains owned by caller, transform never edits it
    }
    void ridge_case() {
        auto f = fixture(3, 1);
        f.heights = {0, 100, 0};
        ClimateContext c = example_context();
        c.frame = {1000, 1000, 10, 0, 0, 0, RowNorth::decreasing};
        c.calendar.tilt_degrees = 0;
        c.model = {1000, 1, 270, 10, .006, 0};
        for (auto& s : f.surface)
            s.albedo = 0;
        auto a = make(f, c, 12);
        near(a.cells()[1].seasonal_reference_kelvin, 270 + 100 / pi);
        near(a.cells()[1].air_reference_kelvin, 270 + 100 / pi - 6);
        near(a.cells()[1].instant_target_kelvin, 364);
        near(a.cells()[1].absorbed_w_m2, 1000);
        near(a.cells()[0].east_slope, 1);
        near(a.cells()[2].east_slope, -1);
        CHECK(a.cells()[0].absorbed_w_m2 < a.cells()[1].absorbed_w_m2);
        auto night = make(f, c, 0);
        for (const auto& cell : night.cells()) {
            CHECK(cell.absorbed_w_m2 == 0);
            CHECK(cell.surface_cosine == 0);
        }
        near(night.cells()[1].seasonal_reference_kelvin, a.cells()[1].seasonal_reference_kelvin);
        near(night.cells()[1].instant_target_kelvin, 264);
        c.model.lapse_kelvin_per_metre = 0;
        auto no_lapse = make(f, c);
        near(no_lapse.cells()[1].air_reference_kelvin, no_lapse.cells()[1].seasonal_reference_kelvin);
    }
    void surface_case() {
        auto f = fixture(3, 1);
        f.surface[0] = {0, 1, .1, .01, 100, 1000};
        f.surface[1] = {.25, .25, .3, .1, 100, 1000};
        f.surface[2] = {0, 0, .5, 1, 100, 1000};
        auto a = make(f);
        near(a.cells()[0].response_seconds, 1000);
        near(a.cells()[1].response_seconds, 550);
        near(a.cells()[2].response_seconds, 100);
        near(a.measurements().finite_water_cell_equivalents, .25);
        near(a.measurements().replenished_water_cell_equivalents, 1.25);
        CHECK(a.cells()[0].seasonal_reference_kelvin > a.cells()[2].seasonal_reference_kelvin);
        ClimateContext dark = example_context();
        dark.model.transmission = 0;
        auto no_solar = make(f, dark);
        for (const auto& cell : no_solar.cells()) {
            CHECK(cell.absorbed_w_m2 == 0);
            near(cell.seasonal_reference_kelvin, dark.model.cooling_floor_kelvin);
        }
        auto altered = f;
        altered.surface[0].roughness_metres = 20;
        auto b = make(altered);
        near(a.cells()[0].air_reference_kelvin, b.cells()[0].air_reference_kelvin); // roughness is retained, no flow solver
        altered.surface[0].replenished_water_fraction = 0;
        b = make(altered);
        near(b.cells()[0].response_seconds, 100);
    }
    void generated_case() {
        for (Index seed : {1U, 42U, 91U}) {
            auto e = generate_elevation(std::array<Index, 2>{17, 11}, {seed, 0x67656f6772617068, 7}, {3, -80, 120}, elevation_hard_limits)
                         .value();
            const auto original = std::vector<Height>(e.heights().begin(), e.heights().end());
            std::vector<ClimateSurface> surface(187);
            for (std::size_t i = 0; i < 187; ++i)
                if (i % 17 < 4)
                    surface[i].replenished_water_fraction = 1;
            ClimateContext c = example_context();
            c.expected_revision = {7, 9};
            auto summer = derive_climate(e, surface, {7, 9}, c, 90 * 24 + 12, budget).value();
            auto winter = derive_climate(e, surface, {7, 9}, c, 270 * 24 + 12, budget).value();
            ProcGenClimateChecks::inspect({e.descriptor().domain, e.heights(), surface, {7, 9}}, summer);
            ProcGenClimateChecks::inspect({e.descriptor().domain, e.heights(), surface, {7, 9}}, winter);
            CHECK(summer.descriptor().elevation.value() == e.descriptor());
            CHECK(summer.cells()[93].air_reference_kelvin > winter.cells()[93].air_reference_kelvin);
            CHECK(summer.measurements().mean_daily_cosine > winter.measurements().mean_daily_cosine);
            CHECK(std::ranges::equal(e.heights(), original));
            CHECK(same(summer, derive_climate(e, surface, {7, 9}, c, 90 * 24 + 12, budget).value()));
            auto changed_budget = budget;
            changed_budget.work = climate_requirements(e.descriptor().domain)->work;
            auto admitted = derive_climate(e, surface, {7, 9}, c, 90 * 24 + 12, changed_budget).value();
            CHECK(std::ranges::equal(summer.cells(), admitted.cells()));
            auto unrelated = IntegerStream::for_stage({seed, 0x1234, 9});
            for (unsigned i = 0; i < 99; ++i)
                static_cast<void>(unrelated.next());
            CHECK(same(summer, derive_climate(e, surface, {7, 9}, c, 90 * 24 + 12, budget).value()));
        }
    }
    void invalid_case() {
        auto f = fixture();
        ClimateContext c = example_context();
        ClimateContext missing;
        error_is(derive_climate(f.input(), missing, 0, budget), FailureKind::invalid_input, ClimateReason::revision);
        missing.expected_revision = {1, 1};
        error_is(derive_climate(f.input(), missing, 0, budget), FailureKind::invalid_input, ClimateReason::frame);
        missing.frame = c.frame;
        error_is(derive_climate(f.input(), missing, 0, budget), FailureKind::invalid_input, ClimateReason::calendar);
        auto in = f.input();
        in.heights = in.heights.first(8);
        error_is(derive_climate(in, c, 0, budget), FailureKind::invalid_input, ClimateReason::shape);
        in = f.input();
        in.surface = in.surface.first(8);
        error_is(derive_climate(in, c, 0, budget), FailureKind::invalid_input, ClimateReason::shape);
        in = f.input();
        in.revision.terrain = 2;
        error_is(derive_climate(in, c, 0, budget), FailureKind::invalid_input, ClimateReason::revision);
        c.expected_revision.terrain = 0;
        error_is(derive_climate(f.input(), c, 0, budget), FailureKind::invalid_input, ClimateReason::revision);
        c = example_context();
        for (double v : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -1.0}) {
            c.frame.dx_metres = v;
            error_is(derive_climate(f.input(), c, 0, budget), FailureKind::invalid_input, ClimateReason::frame);
            c = example_context();
            c.frame.tick_metres = v;
            error_is(derive_climate(f.input(), c, 0, budget), FailureKind::invalid_input, ClimateReason::frame);
            c = example_context();
            c.calendar.tilt_degrees = v;
            error_is(derive_climate(f.input(), c, 0, budget), FailureKind::invalid_input, ClimateReason::calendar);
            c = example_context();
            c.model.cooling_w_m2_kelvin = v;
            error_is(derive_climate(f.input(), c, 0, budget), FailureKind::invalid_input, ClimateReason::model);
            c = example_context();
            f.surface[8].albedo = v;
            ClimateResult<ClimateBaseline> bad = std::unexpected(ClimateFailure{});
            {
                Deny deny;
                bad = derive_climate(f.input(), c, 0, budget);
            }
            error_is(bad, FailureKind::invalid_input, ClimateReason::surface);
            CHECK(bad.error().work == 64 + 8 * 9);
        }
        f = fixture();
        f.surface[0].finite_water_fraction = .6;
        f.surface[0].replenished_water_fraction = .5;
        error_is(derive_climate(f.input(), c, 0, budget), FailureKind::invalid_input, ClimateReason::surface);
        f = fixture();
        f.surface[0].land_response_seconds = 0;
        error_is(derive_climate(f.input(), c, 0, budget), FailureKind::invalid_input, ClimateReason::surface);
        f = fixture();
        c.frame.north = static_cast<RowNorth>(0);
        error_is(derive_climate(f.input(), c, 0, budget), FailureKind::invalid_input, ClimateReason::frame);
        c = example_context();
        for (auto version : {0U, 2U}) {
            c.model_version = version;
            error_is(derive_climate(f.input(), c, 0, budget), FailureKind::unsupported, ClimateReason::version);
            c = example_context();
            c.numerics_version = version;
            error_is(derive_climate(f.input(), c, 0, budget), FailureKind::unsupported, ClimateReason::version);
            c = example_context();
            c.surface_version = version;
            error_is(derive_climate(f.input(), c, 0, budget), FailureKind::unsupported, ClimateReason::version);
            c = example_context();
        }
        for (auto invalid :
             {SolarCalendar{0, 360, 1, 0, 20}, SolarCalendar{86'400, 0, 1, 0, 20}, SolarCalendar{86'400, 360, 0, 0, 20},
              SolarCalendar{86'400, 360, 1, 0, 90}, SolarCalendar{std::numeric_limits<Index>::max(), 360, 1, 0, 20}})
            error_is(sample_sun(invalid, 0, 0, 0), FailureKind::invalid_input, ClimateReason::calendar);
        error_is(sample_sun(c.calendar, 0, 91, 0), FailureKind::invalid_input, ClimateReason::frame);
        error_is(sample_sun(c.calendar, 0, 0, 181), FailureKind::invalid_input, ClimateReason::frame);
        error_is(sample_sun(c.calendar, 0, std::numeric_limits<double>::quiet_NaN(), 0), FailureKind::invalid_input, ClimateReason::frame);
        const double nan = std::numeric_limits<double>::quiet_NaN();
        f = fixture();
        c = example_context();
        for (auto member :
             {&RegionalFrame::dx_metres, &RegionalFrame::dy_metres, &RegionalFrame::tick_metres, &RegionalFrame::datum_metres,
              &RegionalFrame::centre_latitude_degrees, &RegionalFrame::centre_longitude_degrees}) {
            c.frame.*member = nan;
            error_is(derive_climate(f.input(), c, 0, budget), FailureKind::invalid_input, ClimateReason::frame);
            c = example_context();
        }
        for (auto member :
             {&ClimateModel::solar_w_m2, &ClimateModel::transmission, &ClimateModel::cooling_floor_kelvin,
              &ClimateModel::cooling_w_m2_kelvin, &ClimateModel::lapse_kelvin_per_metre, &ClimateModel::reference_metres}) {
            c.model.*member = nan;
            error_is(derive_climate(f.input(), c, 0, budget), FailureKind::invalid_input, ClimateReason::model);
            c = example_context();
        }
        for (auto member :
             {&ClimateSurface::finite_water_fraction, &ClimateSurface::replenished_water_fraction, &ClimateSurface::albedo,
              &ClimateSurface::roughness_metres, &ClimateSurface::land_response_seconds, &ClimateSurface::water_response_seconds}) {
            f.surface[0].*member = nan;
            error_is(derive_climate(f.input(), c, 0, budget), FailureKind::invalid_input, ClimateReason::surface);
            f = fixture();
        }
        CHECK(!Domain::checked(std::array<Index, 2>{0, 1}));
        CHECK(!Domain::checked(std::span<const Index>{}));
        auto one = Domain::checked(std::array<Index, 1>{1}).value();
        error_is(climate_requirements(one), FailureKind::invalid_input, ClimateReason::dimensions);
    }
    void projection_case() {
        auto f = fixture();
        ClimateContext c = example_context();
        for (double lat : {-90.0, -60.1, 60.1, 90.0}) {
            c.frame.centre_latitude_degrees = lat;
            error_is(derive_climate(f.input(), c, 0, budget), FailureKind::unsupported, ClimateReason::projection);
        }
        c = example_context();
        c.frame.centre_longitude_degrees = 180;
        error_is(derive_climate(f.input(), c, 0, budget), FailureKind::unsupported, ClimateReason::projection);
        auto wide = fixture(128, 1);
        c = example_context();
        c.frame.dx_metres = 2001;
        error_is(derive_climate(wide.input(), c, 0, budget), FailureKind::unsupported, ClimateReason::projection);
        c.frame.dx_metres = 2000;
        CHECK(derive_climate(wide.input(), c, 0, budget));
        f.heights[4] = std::numeric_limits<Height>::max();
        c = example_context();
        ClimateResult<ClimateBaseline> bad = std::unexpected(ClimateFailure{});
        {
            Deny deny;
            bad = derive_climate(f.input(), c, 0, budget);
        }
        error_is(bad, FailureKind::unsupported, ClimateReason::heights);
        f = fixture();
        f.heights[4] = 100;
        c.frame.dx_metres = 1;
        error_is(derive_climate(f.input(), c, 0, budget), FailureKind::unsupported, ClimateReason::heights);
        auto tiny = fixture(1, 1);
        tiny.heights[0] = std::numeric_limits<Height>::min();
        c = example_context();
        c.frame.tick_metres = 1e-6;
        auto a = make(tiny, c);
        near(a.cells()[0].z_metres, -2147.483648);
        near(a.cells()[0].east_slope, 0);
        near(a.cells()[0].north_slope, 0);
        for (const auto dims : {std::array<Index, 2>{1, 1}, std::array<Index, 2>{1, 3}, std::array<Index, 2>{3, 1}}) {
            auto t = fixture(dims[0], dims[1]);
            auto v = make(t);
            CHECK(v.cells().size() == dims[0] * dims[1]);
        }
    }
    void limits_case() {
        auto f = fixture();
        const auto req = climate_requirements(f.d).value();
        ClimateContext c = example_context();
        CHECK(req.storage_bytes == 8192 + 9 * 184);
        CHECK(req.work == 64 + 32 * 9);
        const CaveBudgets exact{9, req.storage_bytes, req.work};
        auto held = derive_climate(f.input(), c, 0, exact).value();
        auto copy = held;
        for (unsigned which = 0; which < 3; ++which) {
            auto cap = exact;
            if (which == 0)
                --cap.cells;
            if (which == 1)
                --cap.storage_bytes;
            if (which == 2)
                --cap.work;
            ClimateResult<ClimateBaseline> r = std::unexpected(ClimateFailure{});
            {
                Deny deny;
                r = derive_climate(f.input(), c, 0, cap);
            }
            error_is(
                r, FailureKind::budget_refused,
                which == 0   ? ClimateReason::cells
                : which == 1 ? ClimateReason::storage
                             : ClimateReason::work
            );
            CHECK(r.error().work == 0);
            CHECK(same(held, copy));
            CHECK(f.heights == std::vector<Height>(9, 0));
        }
        auto over = fixture(129, 1);
        error_is(derive_climate(over.input(), c, 0, budget), FailureKind::budget_refused, ClimateReason::cells);
        auto huge = Domain::checked(std::array<Index, 2>{std::numeric_limits<Index>::max(), 1}).value();
        error_is(climate_requirements(huge), FailureKind::invalid_input, ClimateReason::overflow);
        ClimateResult<ClimateBaseline> bad = std::unexpected(ClimateFailure{});
        {
            Deny deny;
            bad = derive_climate({huge, {}, {}, {1, 1}}, c, 0, budget);
        }
        error_is(bad, FailureKind::invalid_input, ClimateReason::overflow);
        auto full = fixture(128, 128);
        c.frame.dx_metres = 1000;
        c.frame.dy_metres = 1000;
        auto accepted = make(full, c);
        CHECK(accepted.cells().size() == 16384);
        CHECK(accepted.usage().storage_bytes < 32 * 1024 * 1024);
        CHECK(accepted.usage().work < 100'000'000);
        bool thrown = false;
        {
            Deny deny;
            try {
                static_cast<void>(derive_climate(f.input(), example_context(), 0, budget));
            } catch (const std::bad_alloc&) {
                thrown = true;
            }
        }
        CHECK(thrown);
        CHECK(same(held, copy));
    }
    void ownership_case() {
        const auto owned = [] {
            auto e = generate_elevation(std::array<Index, 2>{5, 3}, {42, 17, 9}, {2, -10, 20}, elevation_hard_limits).value();
            std::vector<ClimateSurface> surface(15);
            return derive_climate(e, surface, {1, 1}, example_context(), 12, budget).value();
        }();
        auto copy = owned;
        CHECK(same(owned, copy));
        CHECK(owned.cells().data() != copy.cells().data());
        CHECK(copy.descriptor().elevation.has_value());
        auto literal = fixture(2, 2);
        auto old = make(literal);
        const auto snapshot = old;
        literal.heights[0] = 3;
        literal.surface[0].finite_water_fraction = 1;
        auto later = make(literal);
        CHECK(same(old, snapshot));
        CHECK(!std::ranges::equal(old.cells(), later.cells()));
        literal.surface[0].albedo = 2;
        auto rejected = derive_climate(literal.input(), example_context(), 12, budget);
        error_is(rejected, FailureKind::invalid_input, ClimateReason::surface);
        CHECK(same(old, snapshot));
        CHECK(same(owned, copy));
        auto moved = std::move(copy);
        CHECK(same(owned, moved));
        CHECK(!old.descriptor().elevation.has_value());
    }
}
int main(int argc, char** argv) {
    try {
        if (argc != 2)
            throw std::runtime_error("one focused climate case required");
        const std::string name = argv[1];
        if (name == "climate_sun")
            sun_case();
        else if (name == "climate_seasons")
            seasons_case();
        else if (name == "climate_calendar")
            calendar_case();
        else if (name == "climate_frame")
            frame_case();
        else if (name == "climate_ridge")
            ridge_case();
        else if (name == "climate_surface")
            surface_case();
        else if (name == "climate_generated")
            generated_case();
        else if (name == "climate_invalid")
            invalid_case();
        else if (name == "climate_projection")
            projection_case();
        else if (name == "climate_limits")
            limits_case();
        else if (name == "climate_ownership")
            ownership_case();
        else
            throw std::runtime_error("unknown climate case");
        std::cout << "PASS " << name << ": " << assertions << " assertions\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL climate: " << e.what() << '\n';
        return 1;
    }
}
