#include "weather-kernel.h"
#include "moisture-checks.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string>

namespace {
    using namespace ProcGen;
    namespace C = ProcGenMoistureChecks;
    std::size_t assertions = 0;
    void check(bool v, const char* s, int line) {
        ++assertions;
        if (!v)
            throw std::runtime_error("line " + std::to_string(line) + ": " + s);
    }
#define CHECK(x) check(static_cast<bool>(x), #x, __LINE__)
    void near(double a, double b, double tolerance = 1e-10) {
        CHECK(std::abs(a - b) <= tolerance * std::max({1.0, std::abs(a), std::abs(b)}));
    }
    WeatherModel passive() {
        WeatherModel m;
        m.moisture = C::passive_model();
        m.boundary = MoistureBoundary::periodic;
        m.wave_m_s = 0;
        m.rotation_per_second = 0;
        m.drag_per_second = 0;
        m.roughness_drag_per_metre_second = 0;
        m.background_east_m_s = 0;
        m.terrain_acceleration_m_s2 = 0;
        m.pressure_relaxation_per_second = 0;
        m.thermal_potential_per_kelvin = 0;
        m.air_relaxation_per_second = 0;
        m.thermal_enabled = false;
        return m;
    }
    ClimateBaseline flat(Index w = 3, Index h = 3, Index seconds = 1, Index tick = 0) {
        return C::baseline(
            w, h, std::vector<Height>(static_cast<std::size_t>(w * h)), std::vector<ClimateSurface>(static_cast<std::size_t>(w * h)),
            C::context(100, 100, seconds), tick
        );
    }
    WeatherState start(const ClimateBaseline& b, WeatherModel m = passive()) {
        return start_weather(
                   b, std::vector<MoistureCell>(b.cells().size()), std::vector<FlowCell>(b.cells().size()), m, weather_hard_limits
        )
            .value();
    }
    void ledger(const WeatherState& old, const WeatherAdvance& r) {
        const auto& b = old.baseline();
        double initial = 0, final = 0, rain = 0, source = 0;
        const double unit = b.descriptor().context.frame.dx_metres * b.descriptor().context.frame.dy_metres / 1000;
        for (std::size_t i = 0; i < old.water().size(); ++i) {
            const auto x = old.water()[i], y = r.state.water()[i];
            initial += x.vapour_mm + x.cloud_mm + x.finite_surface_mm;
            final += y.vapour_mm + y.cloud_mm + y.finite_surface_mm;
            rain += r.report.moisture.transfers[i].rain_mm;
            source += r.report.moisture.transfers[i].replenished_mm;
            CHECK(y.vapour_mm >= 0 && y.cloud_mm >= 0 && y.finite_surface_mm >= 0);
        }
        near(r.report.moisture.water.initial_m3, initial * unit);
        near(r.report.moisture.water.final_m3, final * unit);
        near((final + rain) * unit, (initial + source) * unit);
        CHECK(std::abs(r.report.moisture.water.residual_m3) <= r.report.moisture.water.tolerance_m3);
    }
    void core() {
        const auto b = flat();
        auto m = passive();
        m.wave_m_s = 2;
        auto s = start(b, m);
        const auto r = advance_weather(s, b, 3, weather_hard_limits).value();
        CHECK(std::ranges::equal(s.flow(), r.state.flow()));
        CHECK(std::ranges::equal(s.water(), r.state.water()));
        CHECK(r.state.tick() == 3);
        CHECK(r.report.trajectory.size() == 3);
        ledger(s, r);
        auto q = std::vector<MoistureCell>(9);
        q[4].vapour_mm = 5;
        std::vector<FlowCell> f(9);
        for (auto& a : f)
            a.east_m_s = 1;
        m.wave_m_s = 0;
        s = start_weather(b, q, f, m, weather_hard_limits).value();
        const auto moved = advance_weather(s, b, 2, weather_hard_limits).value();
        ledger(s, moved);
        for (auto a : moved.state.water())
            CHECK(a.reference_kelvin == 280);
    }
    void sources() {
        const double ln2 = std::log(2.0);
        near(weather_relax(8, 2, 1, ln2).value(), 5);
        near(weather_relax(4, 1, 1, ln2).value(), 2.5);
        const auto v = weather_rotate(1, 0, std::numbers::pi / 2, 1).value();
        near(v[0], 0);
        near(v[1], -1);
        near(std::hypot(v[0], v[1]), 1);
        const auto opposite = weather_rotate(1, 0, -std::numbers::pi / 2, 1).value();
        near(opposite[1], 1);
        const auto b = flat(1, 1);
        auto m = passive();
        m.drag_per_second = ln2;
        m.background_east_m_s = 1;
        m.pressure_relaxation_per_second = ln2;
        const auto s =
            start_weather(b, std::array<MoistureCell, 1>{}, std::array<FlowCell, 1>{{{280, 8, 4, 0}}}, m, weather_hard_limits).value();
        const auto r = advance_weather(s, b, 1, weather_hard_limits).value();
        near(r.state.flow()[0].east_m_s, 2.5);
        near(r.state.flow()[0].potential, 4);
        // Independent uniform source scalars: thermal-low target and roughness drag.
        m = passive();
        m.pressure_relaxation_per_second = ln2;
        m.thermal_potential_per_kelvin = 1;
        m.roughness_drag_per_metre_second = ln2;
        std::array<ClimateSurface, 1> rough{};
        rough[0].roughness_metres = 1;
        const auto rb = C::baseline(1, 1, std::array<Height, 1>{0}, rough, C::context());
        const auto rs =
            start_weather(rb, std::array<MoistureCell, 1>{}, std::array<FlowCell, 1>{{{300, 0, 4, 0}}}, m, weather_hard_limits).value();
        const auto rr = advance_weather(rs, rb, 1, weather_hard_limits).value();
        near(rr.state.flow()[0].potential, -10);
        near(rr.state.flow()[0].east_m_s, 2);
        for (auto direction : {RowNorth::increasing, RowNorth::decreasing}) {
            auto c = C::context(100, 100, 1);
            c.frame.north = direction;
            const auto slope = C::baseline(1, 3, std::array<Height, 3>{0, 1, 2}, std::array<ClimateSurface, 3>{}, c);
            auto tm = passive();
            tm.terrain_acceleration_m_s2 = .01;
            const auto tr = advance_weather(start(slope, tm), slope, 1, weather_hard_limits).value();
            for (auto cell : tr.state.flow())
                near(cell.north_m_s, direction == RowNorth::increasing ? -.001 : .001);
        }
        for (double latitude : {-30.0, 30.0}) {
            auto c = C::context();
            c.frame.centre_latitude_degrees = latitude;
            const auto at = C::baseline(1, 1, std::array<Height, 1>{0}, std::array<ClimateSurface, 1>{}, c);
            m = passive();
            m.rotation_per_second = .005;
            const auto a =
                start_weather(at, std::array<MoistureCell, 1>{}, std::array<FlowCell, 1>{{{280, 0, 1, 0}}}, m, weather_hard_limits).value();
            const auto next = advance_weather(a, at, 1, weather_hard_limits).value();
            const double f = latitude > 0 ? .005 : -.005;
            near(next.state.flow()[0].east_m_s, std::cos(f));
            near(next.state.flow()[0].north_m_s, -std::sin(f));
            near(std::hypot(next.state.flow()[0].east_m_s, next.state.flow()[0].north_m_s), 1);
        }
    }
    void wave() {
        const auto d = Domain::checked(std::array<Index, 2>{3, 1}).value();
        auto frame = C::context().frame;
        frame.dx_metres = 1;
        frame.dy_metres = 1;
        const std::array<FlowCell, 3> old{{{280, 0, 0, 0}, {280, 2, 0, 0}, {280, 0, 0, 0}}};
        std::vector<FlowCell> next(3);
        Work work(1000);
        CHECK(WeatherDetail::wave(old, next, d, frame, 1, .1, MoistureBoundary::periodic, work));
        near(next[0].potential, .1);
        near(next[1].potential, 1.8);
        near(next[2].potential, .1);
        near(next[0].east_m_s, -.1);
        near(next[1].east_m_s, 0);
        near(next[2].east_m_s, .1);
        CHECK(work.used() == 18);
        const auto held = next;
        CHECK(!WeatherDetail::wave(old, next, d, frame, 1, 1, MoistureBoundary::periodic, work));
        CHECK(next == held);
        const auto vertical = Domain::checked(std::array<Index, 2>{1, 3}).value();
        for (auto direction : {RowNorth::increasing, RowNorth::decreasing}) {
            frame.north = direction;
            CHECK(WeatherDetail::wave(old, next, vertical, frame, 1, .1, MoistureBoundary::periodic, work));
            near(next[0].north_m_s, direction == RowNorth::increasing ? -.1 : .1);
            near(next[2].north_m_s, direction == RowNorth::increasing ? .1 : -.1);
        }
        frame.north = RowNorth::decreasing;
        const auto line = Domain::checked(std::array<Index, 2>{32, 1}).value();
        std::vector<FlowCell> a(32), b(32);
        const double k = 2 * std::numbers::pi / 32, c = .2, dt = .1;
        for (std::size_t i = 0; i < 32; ++i) {
            const double q = std::sin(k * static_cast<double>(i));
            a[i] = {280, c * q, q, 0};
        }
        Work w(1000000);
        for (unsigned step = 0; step < 100; ++step) {
            CHECK(WeatherDetail::wave(a, b, line, frame, c, dt, MoistureBoundary::periodic, w));
            a.swap(b);
        }
        std::complex<double> coefficient = 0;
        double error = 0;
        for (std::size_t i = 0; i < 32; ++i) {
            const double x = static_cast<double>(i), exact = std::sin(k * x - c * k * 10);
            coefficient += (a[i].potential / c) * std::exp(std::complex<double>(0, -k * x));
            error += std::pow(a[i].potential / c - exact, 2);
        }
        const auto ratio = coefficient / std::complex<double>(0, -16);
        const auto g = std::complex<double>(.98, 0) + .02 * std::exp(std::complex<double>(0, -k));
        const auto expected = std::pow(g, 100);
        near(std::abs(ratio - expected), 0, 1e-11);
        CHECK(std::abs(1 - std::abs(ratio)) < .05);
        CHECK(std::abs(std::arg(ratio) + c * k * 10) < .01);
        CHECK(std::sqrt(error / 32) < .04);
        std::cout << "Wave reference: amplitude=" << std::abs(ratio) << " phase_error_rad=" << std::abs(std::arg(ratio) + c * k * 10)
                  << " rms_error=" << std::sqrt(error / 32) << '\n';
    }
    void thermal() {
        auto c = C::context(1000, 1000, 3600);
        c.frame.centre_latitude_degrees = 0;
        c.calendar.tilt_degrees = 0;
        std::array<ClimateSurface, 1> land{}, water{};
        land[0].land_response_seconds = 3600;
        water = land;
        water[0].replenished_water_fraction = 1;
        water[0].water_response_seconds = 14400;
        auto m = passive();
        m.thermal_enabled = true;
        const auto b = C::baseline(1, 1, std::array<Height, 1>{0}, land, c, 12);
        auto s = start(b, m);
        const auto r = advance_weather(s, b, 1, weather_hard_limits).value();
        near(
            r.state.flow()[0].surface_kelvin, b.cells()[0].instant_target_kelvin + (280 - b.cells()[0].instant_target_kelvin) * std::exp(-1)
        );
        const auto lb = C::baseline(1, 1, std::array<Height, 1>{0}, land, c), wb = C::baseline(1, 1, std::array<Height, 1>{0}, water, c);
        auto
            l = start_weather(lb, std::array<MoistureCell, 1>{}, std::array<FlowCell, 1>{{{260, 0, 0, 0}}}, m, weather_hard_limits).value(),
            w = start_weather(wb, std::array<MoistureCell, 1>{}, std::array<FlowCell, 1>{{{260, 0, 0, 0}}}, m, weather_hard_limits).value();
        double lr = 260, wr = 260;
        std::vector<double> ls, ws;
        for (Index tick = 0; tick < 72; ++tick) {
            const double sunlight = std::max(0.0, std::cos(2 * std::numbers::pi * (static_cast<double>(tick % 24) - 12) / 24));
            const double target = 260 + 1361 * .65 * .75 / 15 * sunlight;
            lr = target + (lr - target) * std::exp(-1);
            wr = target + (wr - target) * std::exp(-.25);
            auto la = advance_weather(l, lb, 1, weather_hard_limits).value(), wa = advance_weather(w, wb, 1, weather_hard_limits).value();
            near(la.state.flow()[0].surface_kelvin, lr);
            near(wa.state.flow()[0].surface_kelvin, wr);
            l = std::move(la.state);
            w = std::move(wa.state);
            if (tick >= 48) {
                ls.push_back(lr);
                ws.push_back(wr);
            }
        }
        const double ampL = *std::max_element(ls.begin(), ls.end()) - *std::min_element(ls.begin(), ls.end()),
                     ampW = *std::max_element(ws.begin(), ws.end()) - *std::min_element(ws.begin(), ws.end());
        const auto lagL = std::max_element(ls.begin(), ls.end()) - ls.begin(), lagW = std::max_element(ws.begin(), ws.end()) - ws.begin();
        near(ampL, 42.628406009857144);
        near(ampW, 31.98735163950215);
        CHECK(ampW < ampL);
        CHECK(lagW > lagL);
        std::cout << "Thermal72h: land_range_K=" << ampL << " water_range_K=" << ampW << " peak_tick_land=" << lagL << " water=" << lagW
                  << '\n';
    }
    void core_limits() {
        const auto b = flat(1, 1);
        auto s = start(b);
        auto r = advance_weather(s, b, 2, weather_hard_limits).value();
        auto cap = weather_hard_limits;
        cap.work = r.state.usage().work;
        cap.storage_bytes = r.state.usage().storage_bytes;
        cap.cells = 1;
        cap.ticks = 2;
        cap.substeps_per_tick = r.state.usage().substeps_per_tick;
        CHECK(advance_weather(s, b, 2, cap));
        for (unsigned i = 0; i < 5; ++i) {
            auto x = cap;
            if (i == 0)
                --x.cells;
            if (i == 1)
                --x.storage_bytes;
            if (i == 2)
                --x.work;
            if (i == 3)
                --x.ticks;
            if (i == 4)
                --x.substeps_per_tick;
            CHECK(!advance_weather(s, b, 2, x));
        }
        CHECK(!advance_weather(s, b, 0, weather_hard_limits));
        CHECK(!advance_weather(s, b, 11, weather_hard_limits));
        auto m = passive();
        m.thermal_enabled = true;
        m.use_surface_response = false;
        m.uniform_response_seconds = 1;
        const auto stiff = flat(1, 1, 1000);
        const auto initial = start(stiff, m);
        const auto refused = advance_weather(initial, stiff, 1, weather_hard_limits);
        CHECK(!refused);
        CHECK(refused.error().reason == WeatherReason::substeps);
        CHECK(s.tick() == 0);
        CHECK(s.flow()[0].surface_kelvin == 280);
    }
    WeatherModel forced(MoistureBoundary boundary = MoistureBoundary::closed) {
        auto m = passive();
        m.boundary = boundary;
        m.forcing.semantic = {42, 0x77696e64, 7};
        m.forcing.count = 2;
        m.forcing.scales[0] = {4, 8, .25};
        m.forcing.scales[1] = {8, 16, .1};
        m.forcing.origin_east_metres = 10000;
        m.forcing.origin_north_metres = 20000;
        return m;
    }
    void forcing() {
        for (auto boundary : {MoistureBoundary::closed, MoistureBoundary::periodic})
            for (auto direction : {RowNorth::increasing, RowNorth::decreasing}) {
                auto c = C::context(100, 200, 1);
                c.frame.north = direction;
                const auto b = C::baseline(8, 8, std::vector<Height>(64), std::vector<ClimateSurface>(64), c);
                const auto m = forced(boundary);
                const auto r = advance_weather(start(b, m), b, 1, weather_hard_limits).value();
                const auto e = r.state.forcing_east(), n = r.state.forcing_north();
                double maximum = 0;
                for (std::size_t y = 0; y < 8; ++y)
                    for (std::size_t x = 0; x < 8; ++x) {
                        const double sign = direction == RowNorth::increasing ? 1 : -1;
                        const double div = (e[x + 1 + y * 9] - e[x + y * 9]) / 100 + sign * (n[x + (y + 1) * 8] - n[x + y * 8]) / 200;
                        near(div, 0, 1e-17);
                        maximum = std::max(maximum, std::abs(e[x + y * 9]));
                    }
                CHECK(maximum > 1e-6);
                for (std::size_t i = 0; i < 8; ++i) {
                    if (boundary == MoistureBoundary::closed) {
                        CHECK(e[i * 9] == 0 && e[8 + i * 9] == 0);
                        CHECK(n[i] == 0 && n[i + 64] == 0);
                    } else {
                        CHECK(e[i * 9] == e[8 + i * 9]);
                        CHECK(n[i] == n[i + 64]);
                    }
                }
            }
        const auto b = flat(8, 8);
        auto a = forced();
        a.forcing.scales[0].potential_m2_s2 = 0;
        a.forcing.scales[1].potential_m2_s2 = 0;
        auto z = a;
        z.forcing.count = 0;
        const auto zero = advance_weather(start(b, z), b, 2, weather_hard_limits).value();
        const auto amplitude0 = advance_weather(start(b, a), b, 2, weather_hard_limits).value();
        CHECK(std::ranges::equal(zero.state.flow(), amplitude0.state.flow()));
        CHECK(std::ranges::equal(zero.state.water(), amplitude0.state.water()));
    }
    void replay() {
        const auto b = flat(8, 8);
        const auto m = forced();
        const auto s = start(b, m);
        const auto all = advance_weather(s, b, 4, weather_hard_limits).value();
        const auto first = advance_weather(s, b, 2, weather_hard_limits).value();
        const auto second = advance_weather(first.state, b, 2, weather_hard_limits).value();
        CHECK(std::ranges::equal(all.state.flow(), second.state.flow()));
        CHECK(std::ranges::equal(all.state.water(), second.state.water()));
        CHECK(std::ranges::equal(all.state.forcing_east(), second.state.forcing_east()));
        CHECK(all.state.tick() == second.state.tick());
        auto cosmetic = m;
        cosmetic.forcing.cosmetic = {123, 456, 789};
        auto unrelated = IntegerStream::for_stage({42, 0x6f74686572, 7});
        for (int i = 0; i < 100; ++i)
            static_cast<void>(unrelated.next());
        const auto other = advance_weather(start(b, cosmetic), b, 4, weather_hard_limits).value();
        CHECK(std::ranges::equal(all.state.flow(), other.state.flow()));
        CHECK(std::ranges::equal(all.state.water(), other.state.water()));
        near(
            all.report.moisture.water.rain_export_m3,
            first.report.moisture.water.rain_export_m3 + second.report.moisture.water.rain_export_m3
        );
        const auto repeated = advance_weather(s, b, 4, weather_hard_limits).value();
        CHECK(std::ranges::equal(all.state.flow(), repeated.state.flow()));
        auto changed = m;
        changed.forcing.origin_east_metres += 17;
        const auto moved = advance_weather(start(b, changed), b, 4, weather_hard_limits).value();
        CHECK(!std::ranges::equal(all.state.flow(), moved.state.flow()));
    }
    void coupled() {
        const auto e =
            generate_elevation(std::array<Index, 2>{8, 6}, {42, 0x67656f6772617068, 7}, {3, -80, 120}, elevation_hard_limits).value();
        auto c = C::context(2000, 2000, 600);
        std::vector<ClimateSurface> surface(48);
        std::vector<MoistureCell> q(48);
        std::vector<FlowCell> f(48);
        for (std::size_t i = 0; i < 48; ++i) {
            surface[i].land_response_seconds = 3600;
            surface[i].water_response_seconds = 14400;
            if (i % 8 == 0)
                surface[i].replenished_water_fraction = 1;
            if (i % 8 == 1)
                q[i].vapour_mm = 9;
            f[i].east_m_s = 1;
        }
        surface[27].finite_water_fraction = 1;
        q[27].finite_surface_mm = .02;
        const auto b = derive_climate(e, surface, {1, 1}, c, 36, climate_hard_limits).value();
        for (std::size_t i = 0; i < 48; ++i)
            f[i].surface_kelvin = 280 - c.model.lapse_kelvin_per_metre * b.cells()[i].z_metres;
        WeatherModel m;
        m.forcing = forced().forcing;
        m.moisture.surface_evaporation_mm_per_second = .00001;
        auto s = start_weather(b, q, f, m, weather_hard_limits).value();
        const auto initial = s;
        double rain = 0, external = 0;
        for (int interval = 0; interval < 4; ++interval) {
            auto r = advance_weather(s, b, 10, weather_hard_limits);
            CHECK(r);
            ledger(s, *r);
            rain += r->report.moisture.water.rain_export_m3;
            external += r->report.moisture.water.replenished_m3;
            for (auto v : r->state.flow()) {
                CHECK(std::isfinite(v.potential));
                CHECK(std::hypot(v.east_m_s, v.north_m_s) <= m.maximum_wind_m_s);
            }
            s = std::move(r->state);
        }
        CHECK(s.tick() == 76);
        CHECK(rain > 0 && external > 0);
        CHECK(!std::ranges::equal(initial.water(), s.water()));
        CHECK(!std::ranges::equal(initial.flow(), s.flow()));
        // Grouping holds with active thermal/phase/wave sources, not just passive transport.
        const auto direct = advance_weather(initial, b, 10, weather_hard_limits).value();
        auto limit = weather_hard_limits;
        limit.substeps_per_tick = 20;
        const auto exact = advance_weather(initial, b, 10, limit);
        CHECK(exact && exact->state.usage().substeps_per_tick == 20);
        --limit.substeps_per_tick;
        const auto one_short = advance_weather(initial, b, 10, limit);
        CHECK(!one_short && one_short.error().reason == WeatherReason::substeps);
        auto cosmetic = m;
        cosmetic.forcing.cosmetic = {999, 888, 777};
        const auto isolated =
            advance_weather(start_weather(b, q, f, cosmetic, weather_hard_limits).value(), b, 10, weather_hard_limits).value();
        CHECK(std::ranges::equal(direct.state.water(), isolated.state.water()));
        CHECK(std::ranges::equal(direct.state.flow(), isolated.state.flow()));
        CHECK(direct.report.moisture.water.rain_export_m3 == isolated.report.moisture.water.rain_export_m3);
        auto group = advance_weather(initial, b, 4, weather_hard_limits).value();
        auto tail = advance_weather(group.state, b, 6, weather_hard_limits).value();
        CHECK(std::ranges::equal(direct.state.flow(), tail.state.flow()));
        CHECK(std::ranges::equal(direct.state.water(), tail.state.water()));
        near(
            direct.report.moisture.water.rain_export_m3,
            group.report.moisture.water.rain_export_m3 + tail.report.moisture.water.rain_export_m3
        );
    }
    void invalid() {
        const auto b = flat();
        std::vector<MoistureCell> q(9);
        std::vector<FlowCell> f(9);
        const auto m = passive();
        auto bad = [&](WeatherModel x) { CHECK(!start_weather(b, q, f, x, weather_hard_limits)); };
        auto x = m;
        x.version = 2;
        bad(x);
        x = m;
        x.numerics_version = 2;
        bad(x);
        x = m;
        x.wave_m_s = .01;
        bad(x);
        x = m;
        x.maximum_wind_m_s = std::numeric_limits<double>::infinity();
        bad(x);
        x = m;
        x.rotation_per_second = .1;
        bad(x);
        x = m;
        x.forcing.count = 5;
        bad(x);
        x = forced();
        x.forcing.scales[0].wavelength_cells = 3;
        bad(x);
        x = forced();
        x.forcing.scales[0].period_ticks = 0;
        bad(x);
        x = forced();
        x.forcing.origin_east_metres = std::numeric_limits<double>::quiet_NaN();
        bad(x);
        x = forced(MoistureBoundary::periodic);
        bad(x);
        x = m;
        x.forcing.count = 1;
        x.forcing.scales[0].potential_m2_s2 = -1;
        bad(x);
        x = m;
        x.forcing.scales[3].potential_m2_s2 = std::numeric_limits<double>::quiet_NaN();
        bad(x);
        x = m;
        x.moisture.version = 2;
        const auto unknown = start_weather(b, q, f, x, weather_hard_limits);
        CHECK(!unknown && unknown.error().kind == FailureKind::unsupported && unknown.error().reason == WeatherReason::version);
        x = m;
        x.moisture.numerics_version = 2;
        const auto numeric_version = start_weather(b, q, f, x, weather_hard_limits);
        CHECK(
            !numeric_version && numeric_version.error().kind == FailureKind::unsupported &&
            numeric_version.error().reason == WeatherReason::version
        );
        q.pop_back();
        CHECK(!start_weather(b, q, f, m, weather_hard_limits));
        q.resize(9);
        f.pop_back();
        CHECK(!start_weather(b, q, f, m, weather_hard_limits));
        f.resize(9);
        f[0].east_m_s = 9;
        CHECK(!start_weather(b, q, f, m, weather_hard_limits));
        f[0] = {};
        q[0].cloud_mm = -1;
        CHECK(!start_weather(b, q, f, m, weather_hard_limits));
        q[0] = {};
        f[0].potential = std::numeric_limits<double>::quiet_NaN();
        CHECK(!start_weather(b, q, f, m, weather_hard_limits));
        const auto held = start(b);
        auto c = C::context(100, 100, 1);
        c.expected_revision = {2, 1};
        const auto changed = C::baseline(3, 3, std::vector<Height>(9), std::vector<ClimateSurface>(9), c);
        CHECK(!advance_weather(held, changed, 1, weather_hard_limits));
        auto heights = std::vector<Height>(9);
        heights[0] = 1;
        const auto other = C::baseline(3, 3, heights, std::vector<ClimateSurface>(9), C::context(100, 100, 1));
        CHECK(!advance_weather(held, other, 1, weather_hard_limits));
        CHECK(held.tick() == 0);
        const auto atmax = flat(1, 1, 1, std::numeric_limits<Index>::max());
        const auto overflow = advance_weather(start(atmax), atmax, 1, weather_hard_limits);
        CHECK(!overflow && overflow.error().reason == WeatherReason::overflow);
        CHECK(!weather_relax(1, 2, -1, 1));
        CHECK(!weather_rotate(1, 0, 1, 4));
        CHECK(!weather_requirements(Domain::checked(std::array<Index, 1>{1}).value()));
        CHECK(!weather_requirements(Domain::checked(std::array<Index, 2>{std::numeric_limits<Index>::max(), 1}).value()));
    }
    void limits() {
        const auto b = flat(8, 8);
        const auto m = forced();
        const auto s = start(b, m);
        auto cap = weather_hard_limits;
        cap.cells = 64;
        cap.storage_bytes = s.usage().storage_bytes;
        cap.work = s.usage().work;
        CHECK(start_weather(b, s.water(), s.flow(), m, cap));
        for (int i = 0; i < 3; ++i) {
            auto a = cap;
            if (i == 0)
                --a.cells;
            if (i == 1)
                --a.storage_bytes;
            if (i == 2)
                --a.work;
            CHECK(!start_weather(b, s.water(), s.flow(), m, a));
        }
        const auto result = advance_weather(s, b, 2, weather_hard_limits).value();
        cap = weather_hard_limits;
        cap.cells = 64;
        cap.storage_bytes = result.state.usage().storage_bytes;
        cap.work = result.state.usage().work;
        cap.ticks = 2;
        cap.substeps_per_tick = result.state.usage().substeps_per_tick;
        CHECK(advance_weather(s, b, 2, cap));
        for (int i = 0; i < 5; ++i) {
            auto a = cap;
            if (i == 0)
                --a.cells;
            if (i == 1)
                --a.storage_bytes;
            if (i == 2)
                --a.work;
            if (i == 3)
                --a.ticks;
            if (i == 4)
                --a.substeps_per_tick;
            CHECK(!advance_weather(s, b, 2, a));
        }
        // Schedule search consumes work even when no substep count can be admitted.
        const auto stiff = flat(1, 1, 1000);
        auto model = passive();
        model.thermal_enabled = true;
        model.use_surface_response = false;
        model.uniform_response_seconds = 1;
        const auto ss = start(stiff, model);
        const auto full = advance_weather(ss, stiff, 1, weather_hard_limits);
        CHECK(!full && full.error().reason == WeatherReason::substeps);
        auto a = weather_hard_limits;
        a.work = full.error().work - 1;
        const auto runtime = advance_weather(ss, stiff, 1, a);
        CHECK(!runtime && runtime.error().reason == WeatherReason::work);
        CHECK(runtime.error().work > 64);
        // Valid input can accelerate past its declared speed cap; refuse whole candidate.
        auto fast = passive();
        fast.wave_m_s = 2;
        fast.maximum_wind_m_s = 1;
        std::vector<FlowCell> f(64);
        f[1].potential = 10000;
        const auto initial = start_weather(b, s.water(), f, fast, weather_hard_limits).value();
        const auto refused = advance_weather(initial, b, 1, weather_hard_limits);
        CHECK(!refused && refused.error().reason == WeatherReason::speed);
        CHECK(initial.flow()[1].potential == 10000);
        CHECK(std::ranges::equal(s.flow(), start(b, m).flow()));
    }
    void ownership() {
        auto make = []() {
            const auto b = flat(8, 8);
            auto q = std::vector<MoistureCell>(64);
            q[9].vapour_mm = 5;
            auto f = std::vector<FlowCell>(64);
            return start_weather(b, q, f, forced(), weather_hard_limits).value();
        };
        const auto held = make();
        auto copy = held;
        const auto out = advance_weather(copy, copy.baseline(), 3, weather_hard_limits).value();
        CHECK(held.water()[9].vapour_mm == 5);
        CHECK(held.tick() == 0);
        CHECK(copy.flow().data() != held.flow().data());
        CHECK(copy.water().data() != held.water().data());
        CHECK(copy.baseline().cells().data() != held.baseline().cells().data());
        CHECK(out.state.flow().data() != held.flow().data());
        const auto saved = out;
        auto cap = weather_hard_limits;
        cap.work = 0;
        CHECK(!advance_weather(out.state, out.state.baseline(), 1, cap));
        CHECK(std::ranges::equal(saved.state.flow(), out.state.flow()));
        CHECK(saved.report.moisture.transfers.data() != out.report.moisture.transfers.data());
        CHECK(saved.report.trajectory.size() == 3);
    }

}
int main(int argc, char** argv) {
    try {
        if (argc != 2)
            throw std::runtime_error("one weather case required");
        const std::string n = argv[1];
        if (n == "weather_core")
            core();
        else if (n == "weather_sources")
            sources();
        else if (n == "weather_wave")
            wave();
        else if (n == "weather_thermal")
            thermal();
        else if (n == "weather_core_limits")
            core_limits();
        else if (n == "weather_forcing")
            forcing();
        else if (n == "weather_replay")
            replay();
        else if (n == "weather_coupled")
            coupled();
        else if (n == "weather_invalid")
            invalid();
        else if (n == "weather_limits")
            limits();
        else if (n == "weather_ownership")
            ownership();
        else
            throw std::runtime_error("unknown weather case");
        std::cout << "PASS " << n << ": " << assertions << " assertions\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL weather: " << e.what() << '\n';
        return 1;
    }
}
