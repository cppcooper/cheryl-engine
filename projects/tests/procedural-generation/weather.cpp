#include "weather.h"
#include "weather-kernel.h"
#include "moisture-kernel.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <utility>

namespace ProcGen {
    namespace {
        struct Stop {
            WeatherFailure failure;
        };
        [[noreturn]] void fail(FailureKind k, WeatherReason r) {
            throw Stop{{k, r}};
        }
        void charge(Work& w, Index n = 1) {
            if (!w.consume(n))
                fail(FailureKind::budget_refused, WeatherReason::work);
        }
        Index add(Index a, Index b) {
            if (a > std::numeric_limits<Index>::max() - b)
                fail(FailureKind::invalid_input, WeatherReason::overflow);
            return a + b;
        }
        Index mul(Index a, Index b) {
            if (b && a > std::numeric_limits<Index>::max() / b)
                fail(FailureKind::invalid_input, WeatherReason::overflow);
            return a * b;
        }
        bool range(double x, double a, double b) {
            return std::isfinite(x) && x >= a && x <= b;
        }
        WeatherRequirements needed(const Domain& d) {
            if (d.dimensions() != 2)
                fail(FailureKind::invalid_input, WeatherReason::dimensions);
            const auto c = d.cells(), w = d.extents()[0], h = d.extents()[1];
            const auto f = add(mul(add(w, 1), h), mul(w, add(h, 1))), v = mul(add(w, 1), add(h, 1));
            return {
                c,
                f,
                v,
                add(weather_fixed_storage, add(mul(248, c), mul(8, f))),
                add(weather_fixed_storage, add(mul(368, c), add(mul(16, f), mul(8, v)))),
                add(64, add(mul(8, c), f))
            };
        }
        void admit(const Domain& d, const WeatherRequirements& r, MoistureBudgets b, bool advancing, Index minimum) {
            if (r.cells > std::min(b.cells, weather_hard_limits.cells) || d.extents()[0] > 128 || d.extents()[1] > 128)
                fail(FailureKind::budget_refused, WeatherReason::cells);
            if ((advancing ? r.advance_storage_bytes : r.start_storage_bytes) >
                std::min(b.storage_bytes, weather_hard_limits.storage_bytes))
                fail(FailureKind::budget_refused, WeatherReason::storage);
            if (minimum > std::min(b.work, weather_hard_limits.work))
                fail(FailureKind::budget_refused, WeatherReason::work);
        }
        void model_ok(const WeatherModel& m, const Domain& d) {
            if (m.version != 1 || m.numerics_version != 1)
                fail(FailureKind::unsupported, WeatherReason::version);
            if (auto capacity = moisture_capacity(280, m.moisture); !capacity)
                fail(
                    capacity.error().kind,
                    capacity.error().reason == MoistureReason::version ? WeatherReason::version : WeatherReason::model
                );
            if (m.boundary != MoistureBoundary::closed && m.boundary != MoistureBoundary::periodic)
                fail(FailureKind::unsupported, WeatherReason::model);
            if (!range(m.wave_m_s, 0, 50) || (m.wave_m_s != 0 && m.wave_m_s < .1) || !range(m.maximum_wind_m_s, .1, 50) ||
                !range(m.rotation_per_second, -.01, .01) || !range(m.drag_per_second, 0, 1) ||
                !range(m.roughness_drag_per_metre_second, 0, 1) || !range(m.background_east_m_s, -50, 50) ||
                !range(m.background_north_m_s, -50, 50) || std::hypot(m.background_east_m_s, m.background_north_m_s) > m.maximum_wind_m_s ||
                !range(m.terrain_acceleration_m_s2, 0, .01) || !range(m.pressure_relaxation_per_second, 0, 1) ||
                !range(m.thermal_potential_per_kelvin, 0, 10) || !range(m.air_relaxation_per_second, 0, 1) ||
                !range(m.uniform_response_seconds, 1, 1e8))
                fail(FailureKind::invalid_input, WeatherReason::model);
            if (m.forcing.count > 4)
                fail(FailureKind::unsupported, WeatherReason::forcing);
            if (!range(m.forcing.origin_east_metres, -1e7, 1e7) || !range(m.forcing.origin_north_metres, -1e7, 1e7))
                fail(FailureKind::invalid_input, WeatherReason::forcing);
            for (Index k = 0; k < m.forcing.scales.size(); ++k) {
                const auto& a = m.forcing.scales[static_cast<std::size_t>(k)];
                if (!range(a.potential_m2_s2, 0, 100))
                    fail(FailureKind::invalid_input, WeatherReason::forcing);
                if (a.wavelength_cells < 4 || a.wavelength_cells > 128 || a.period_ticks < 4 || a.period_ticks > 1'000'000)
                    fail(FailureKind::unsupported, WeatherReason::forcing);
                if (k < m.forcing.count && a.potential_m2_s2 > 0 && m.boundary == MoistureBoundary::periodic &&
                    (d.extents()[0] % a.wavelength_cells != 0 || d.extents()[1] % a.wavelength_cells != 0))
                    fail(FailureKind::unsupported, WeatherReason::forcing);
            }
        }
        double lapse(const ClimateBaseline& b, std::size_t i) {
            const auto& m = b.descriptor().context.model;
            return m.lapse_kelvin_per_metre * (b.cells()[i].z_metres - m.reference_metres);
        }
        bool valid_flow(const FlowCell& f, const ClimateBaseline& b, std::size_t i, const WeatherModel& m) {
            return range(f.surface_kelvin, 1, 1000) && range(f.surface_kelvin + lapse(b, i), 249, 601) &&
                   range(f.potential, -10000, 10000) && std::isfinite(f.east_m_s) && std::isfinite(f.north_m_s) &&
                   std::hypot(f.east_m_s, f.north_m_s) <= m.maximum_wind_m_s;
        }
        void check_flow(const FlowCell& f, const ClimateBaseline& b, std::size_t i, const WeatherModel& m) {
            if (valid_flow(f, b, i, m))
                return;
            const bool speed =
                std::isfinite(f.east_m_s) && std::isfinite(f.north_m_s) && std::hypot(f.east_m_s, f.north_m_s) > m.maximum_wind_m_s;
            fail(FailureKind::unsupported, speed ? WeatherReason::speed : WeatherReason::numeric);
        }
        void match(const ClimateBaseline& a, const ClimateBaseline& b, Work& work) {
            const auto& x = a.descriptor();
            const auto& y = b.descriptor();
            // Exact relevant witness. Different admission metadata is harmless.
            if (x.domain != y.domain || x.revision != y.revision || x.context != y.context || x.tick != y.tick ||
                x.elevation.has_value() != y.elevation.has_value())
                fail(FailureKind::invalid_input, WeatherReason::context);
            if (x.elevation && (x.elevation->domain != y.elevation->domain || x.elevation->random != y.elevation->random ||
                                x.elevation->requested != y.elevation->requested || x.elevation->algorithm != y.elevation->algorithm ||
                                x.elevation->version != y.elevation->version))
                fail(FailureKind::invalid_input, WeatherReason::context);
            for (std::size_t i = 0; i < a.cells().size(); ++i) {
                charge(work);
                if (a.cells()[i] != b.cells()[i])
                    fail(FailureKind::invalid_input, WeatherReason::context);
            }
        }
        double solar_target(const ClimateBaseline& b, std::size_t i, Index tick) {
            const auto& a = b.cells()[i];
            const auto& c = b.descriptor().context;
            const auto sun = sample_sun(c.calendar, tick, a.latitude_degrees, a.longitude_degrees).value();
            const double norm = std::sqrt(1 + a.east_slope * a.east_slope + a.north_slope * a.north_slope);
            const double light =
                sun.up <= 0 ? 0 : std::clamp((sun.up - a.east_slope * sun.east - a.north_slope * sun.north) / norm, 0.0, 1.0);
            return c.model.cooling_floor_kelvin +
                   c.model.solar_w_m2 * c.model.transmission * (1 - a.surface.albedo) * light / c.model.cooling_w_m2_kelvin - lapse(b, i);
        }
        double water_total(std::span<const MoistureCell> cells) {
            double sum = 0;
            for (const auto& a : cells)
                sum += a.vapour_mm + a.cloud_mm + a.finite_surface_mm;
            return sum;
        }
        void finish(MoistureReport& report, std::span<const MoistureCell> water, double unit) {
            auto& l = report.water;
            l.final_m3 = water_total(water) * unit;
            for (const auto& t : report.transfers) {
                l.finite_evaporation_m3 += t.finite_evaporation_mm * unit;
                l.replenished_m3 += t.replenished_mm * unit;
                l.condensed_m3 += t.condensed_mm * unit;
                l.cloud_evaporated_m3 += t.cloud_evaporated_mm * unit;
                l.rain_export_m3 += t.rain_mm * unit;
            }
            l.residual_m3 = l.final_m3 + l.rain_export_m3 - l.initial_m3 - l.replenished_m3;
            l.tolerance_m3 = 1e-10 * std::max(1.0, l.initial_m3 + l.replenished_m3 + l.rain_export_m3);
            if (!std::isfinite(l.residual_m3) || std::abs(l.residual_m3) > l.tolerance_m3)
                fail(FailureKind::unsupported, WeatherReason::numeric);
        }
        static_assert(sizeof(FlowCell) == 32 && sizeof(MoistureCell) == 32);
        static_assert(4 * sizeof(WeatherState) + 4096 <= weather_fixed_storage);
    }
    std::string_view weather_reason_text(WeatherReason r) {
        switch (r) {
            case WeatherReason::dimensions:
                return "weather requires checked2D";
            case WeatherReason::shape:
                return "weather arrays must equal cells";
            case WeatherReason::context:
                return "incompatible exact weather source/frame/baseline";
            case WeatherReason::initial:
                return "invalid initial water/flow/temperature";
            case WeatherReason::model:
                return "invalid weather parameters/boundary";
            case WeatherReason::overflow:
                return "weather arithmetic/clock overflow";
            case WeatherReason::cells:
                return "weather cell/axis budget refused";
            case WeatherReason::storage:
                return "weather payload budget refused";
            case WeatherReason::work:
                return "weather work refused or exhausted";
            case WeatherReason::ticks:
                return "weather requires1..10 admitted ticks";
            case WeatherReason::substeps:
                return "wave/transport/source constraints exceed substep allowance";
            case WeatherReason::speed:
                return "weather wind exceeds declared speed cap";
            case WeatherReason::numeric:
                return "weather numeric range/residual refused";
            case WeatherReason::version:
                return "unsupported weather version";
            case WeatherReason::forcing:
                return "unsupported forcing";
        }
        return "unknown weather refusal";
    }
    WeatherResult<WeatherRequirements> weather_requirements(const Domain& d) {
        try {
            return needed(d);
        } catch (const Stop& s) {
            return std::unexpected(s.failure);
        }
    }
    WeatherResult<double> weather_relax(double v, double target, double dt, double rate) {
        if (!range(v, -1e5, 1e5) || !range(target, -1e5, 1e5) || !range(dt, 0, 1e6) || !range(rate, 0, 100))
            return std::unexpected(WeatherFailure{FailureKind::invalid_input, WeatherReason::model});
        return target + (v - target) * std::exp(-rate * dt);
    }
    WeatherResult<std::array<double, 2>> weather_rotate(double u, double v, double f, double dt) {
        if (!range(u, -1000, 1000) || !range(v, -1000, 1000) || !range(dt, 0, 1e6) || !std::isfinite(f) ||
            std::abs(f * dt) > std::numbers::pi)
            return std::unexpected(WeatherFailure{FailureKind::invalid_input, WeatherReason::model});
        const double a = f * dt, c = std::cos(a), s = std::sin(a);
        return std::array<double, 2>{c * u + s * v, -s * u + c * v};
    }
    WeatherState::WeatherState(
        ClimateBaseline b,
        std::vector<MoistureCell> q,
        std::vector<FlowCell> f,
        WeatherModel m,
        Index t,
        WeatherUsage u,
        std::vector<double> fe,
        std::vector<double> fn
    )
    : baseline_(std::move(b)),
      water_(std::move(q)),
      flow_(std::move(f)),
      model_(m),
      forcing_east_(std::move(fe)),
      forcing_north_(std::move(fn)),
      tick_(t),
      usage_(u) {}
    WeatherResult<WeatherState> start_weather(
        const ClimateBaseline& b,
        std::span<const MoistureCell> q,
        std::span<const FlowCell> f,
        WeatherModel m,
        MoistureBudgets budgets
    ) {
        Work work(std::min(budgets.work, weather_hard_limits.work));
        try {
            const auto r = needed(b.descriptor().domain);
            if (q.size() != r.cells || f.size() != r.cells)
                fail(FailureKind::invalid_input, WeatherReason::shape);
            admit(b.descriptor().domain, r, budgets, false, r.start_work);
            charge(work, 64);
            model_ok(m, b.descriptor().domain);
            for (std::size_t i = 0; i < q.size(); ++i) {
                charge(work, 4);
                if (!MoistureDetail::valid_cell(q[i], b, i, true) || !valid_flow(f[i], b, i, m))
                    fail(FailureKind::invalid_input, WeatherReason::initial);
            }
            charge(work, add(mul(4, r.cells), r.faces));
            return WeatherState(
                b, {q.begin(), q.end()}, {f.begin(), f.end()}, m, b.descriptor().tick, {work.used(), r.start_storage_bytes, 0, 0},
                std::vector<double>(
                    static_cast<std::size_t>((b.descriptor().domain.extents()[0] + 1) * b.descriptor().domain.extents()[1])
                ),
                std::vector<double>(static_cast<std::size_t>(b.descriptor().domain.extents()[0] * (b.descriptor().domain.extents()[1] + 1)))
            );
        } catch (const Stop& s) {
            auto e = s.failure;
            e.work = work.used();
            return std::unexpected(e);
        }
    }
    WeatherResult<WeatherAdvance>
    advance_weather(const WeatherState& old, const ClimateBaseline& expected, Index ticks, MoistureBudgets budgets) {
        Work work(std::min(budgets.work, weather_hard_limits.work));
        try {
            const auto& b = old.baseline();
            const auto& d = b.descriptor().domain;
            const auto& frame = b.descriptor().context.frame;
            const auto& m = old.model();
            const auto r = needed(d);
            if (ticks == 0)
                fail(FailureKind::invalid_input, WeatherReason::ticks);
            if (ticks > std::min(budgets.ticks, weather_hard_limits.ticks))
                fail(FailureKind::budget_refused, WeatherReason::ticks);
            const auto end = add(old.tick(), ticks), w = d.extents()[0], h = d.extents()[1];
            const auto edges =
                (w - 1) * h + (h - 1) * w + (m.boundary == MoistureBoundary::periodic ? (w > 1 ? h : 0) + (h > 1 ? w : 0) : 0);
            const auto per_step = add(mul(25, r.cells), add(r.faces, add(mul(2, edges), 2)));
            const auto base = add(64, add(mul(12, r.cells), add(mul(2, r.faces), r.vertices)));
            const auto per_tick =
                add(add(mul(10, r.cells), 1), add(mul(2, m.forcing.count), add(mul(r.vertices, add(1, mul(2, m.forcing.count))), r.faces)));
            const auto minimum = add(base, add(4, mul(ticks, add(per_tick, per_step))));
            admit(d, r, budgets, true, minimum);
            charge(work, 64);
            match(b, expected, work);
            double rate = std::max(m.pressure_relaxation_per_second, m.air_relaxation_per_second);
            double max_acceleration = 0;
            for (const auto& a : b.cells()) {
                charge(work, 2);
                max_acceleration = std::max(max_acceleration, m.terrain_acceleration_m_s2 * std::hypot(a.east_slope, a.north_slope));
                rate = std::max(
                    {rate, std::abs(2 * m.rotation_per_second * std::sin(a.latitude_degrees * std::numbers::pi / 180)),
                     m.drag_per_second + m.roughness_drag_per_metre_second * a.surface.roughness_metres,
                     m.thermal_enabled ? 1 / (m.use_surface_response ? a.response_seconds : m.uniform_response_seconds) : 0}
                );
            }
            for (Index k = 0; k < m.forcing.count; ++k)
                max_acceleration += 2 * std::sqrt(2.0) * m.forcing.scales[static_cast<std::size_t>(k)].potential_m2_s2 /
                                    std::min(frame.dx_metres, frame.dy_metres);
            const double seconds = static_cast<double>(b.descriptor().context.calendar.seconds_per_tick);
            const double active = (w > 1 ? 1 / frame.dx_metres : 0) + (h > 1 ? 1 / frame.dy_metres : 0);
            const double wave_inverse = m.boundary == MoistureBoundary::periodic ? active : 1 / frame.dx_metres + 1 / frame.dy_metres;
            Index n = 0;
            double dt = 0;
            for (Index trial = 1; trial <= std::min(budgets.substeps_per_tick, weather_hard_limits.substeps_per_tick); ++trial) {
                charge(work, 4);
                dt = seconds / static_cast<double>(trial);
                if (m.wave_m_s * dt * wave_inverse <= .5 && 2 * m.maximum_wind_m_s * dt * active <= .5 && rate * dt <= 1 &&
                    max_acceleration * dt <= .5 * m.maximum_wind_m_s) {
                    n = trial;
                    break;
                }
            }
            if (!n)
                fail(FailureKind::budget_refused, WeatherReason::substeps);
            const auto exact = add(base, add(mul(4, n), mul(ticks, add(per_tick, mul(n, per_step)))));
            if (exact > std::min(budgets.work, weather_hard_limits.work))
                fail(FailureKind::budget_refused, WeatherReason::work);
            charge(work, add(mul(8, r.cells), add(mul(2, r.faces), r.vertices)));
            ClimateBaseline retained = b;
            std::vector<MoistureCell> water(old.water().begin(), old.water().end()), next_water(static_cast<std::size_t>(r.cells));
            std::vector<FlowCell> flow(old.flow().begin(), old.flow().end()), next_flow(static_cast<std::size_t>(r.cells));
            std::vector<double> targets(static_cast<std::size_t>(r.cells)), outgoing(static_cast<std::size_t>(r.cells));
            const auto ef = (w + 1) * h, nf = w * (h + 1);
            std::vector<double> east(static_cast<std::size_t>(ef)), north(static_cast<std::size_t>(nf));
            std::vector<double> force_east(static_cast<std::size_t>(ef)), force_north(static_cast<std::size_t>(nf)),
                potential(static_cast<std::size_t>(r.vertices));
            WeatherReport report{
                {old.tick(),
                 end,
                 b.descriptor().context.calendar.seconds_per_tick,
                 std::vector<MoistureTransfer>(static_cast<std::size_t>(r.cells)),
                 {}},
                {}
            };
            report.trajectory.reserve(static_cast<std::size_t>(ticks));
            const double unit = frame.dx_metres * frame.dy_metres / 1000;
            report.moisture.water.initial_m3 = water_total(water) * unit;
            for (Index outer = 0; outer < ticks; ++outer) {
                charge(work);
                const auto absolute = old.tick() + outer;
                auto forcing = WeatherDetail::forcing(b, m, absolute, potential, force_east, force_north, work);
                if (!forcing)
                    throw Stop{forcing.error()};
                for (std::size_t i = 0; i < water.size(); ++i) {
                    charge(work, 8);
                    targets[i] = solar_target(b, i, absolute);
                }
                for (Index sub = 0; sub < n; ++sub) {
                    for (std::size_t i = 0; i < water.size(); ++i) {
                        charge(work, 8);
                        auto& f = flow[i];
                        auto& q = water[i];
                        const auto& a = b.cells()[i];
                        const double correction = lapse(b, i);
                        if (m.thermal_enabled)
                            f.surface_kelvin = weather_relax(
                                                   f.surface_kelvin, targets[i], dt,
                                                   1 / (m.use_surface_response ? a.response_seconds : m.uniform_response_seconds)
                            )
                                                   .value();
                        q.reference_kelvin =
                            weather_relax(q.reference_kelvin, f.surface_kelvin + correction, dt, m.air_relaxation_per_second).value();
                        const double target = -m.thermal_potential_per_kelvin * (f.surface_kelvin - (q.reference_kelvin - correction));
                        f.potential = weather_relax(f.potential, target, dt, m.pressure_relaxation_per_second).value();
                        const auto rotated = weather_rotate(
                                                 f.east_m_s, f.north_m_s,
                                                 2 * m.rotation_per_second * std::sin(a.latitude_degrees * std::numbers::pi / 180), dt
                        )
                                                 .value();
                        const Index x = static_cast<Index>(i) % w, y = static_cast<Index>(i) / w;
                        const double acceleration_east = .5 * (force_east[static_cast<std::size_t>(x + y * (w + 1))] +
                                                               force_east[static_cast<std::size_t>(x + 1 + y * (w + 1))]);
                        const double acceleration_north = .5 * (force_north[static_cast<std::size_t>(x + y * w)] +
                                                                force_north[static_cast<std::size_t>(x + (y + 1) * w)]);
                        const double drag = m.drag_per_second + m.roughness_drag_per_metre_second * a.surface.roughness_metres;
                        f.east_m_s = weather_relax(rotated[0], m.background_east_m_s, dt, drag).value() -
                                     m.terrain_acceleration_m_s2 * a.east_slope * dt + acceleration_east * dt;
                        f.north_m_s = weather_relax(rotated[1], m.background_north_m_s, dt, drag).value() -
                                      m.terrain_acceleration_m_s2 * a.north_slope * dt + acceleration_north * dt;
                        check_flow(f, b, i, m);
                    }
                    auto wave = WeatherDetail::wave(flow, next_flow, d, frame, m.wave_m_s, dt, m.boundary, work);
                    if (!wave)
                        throw Stop{wave.error()};
                    flow.swap(next_flow);
                    charge(work);
                    charge(work, r.faces);
                    WeatherDetail::face_winds(flow, d, m.boundary, east, north, work);
                    const FaceWinds wind{east, north, m.boundary};
                    for (Index i = 0; i < r.cells; ++i) {
                        charge(work, 4);
                        check_flow(flow[static_cast<std::size_t>(i)], b, static_cast<std::size_t>(i), m);
                        const auto fractions = MoistureDetail::fractions(wind, d, frame, i, dt);
                        if (fractions.first > 1 || fractions.second > 1)
                            fail(FailureKind::unsupported, WeatherReason::substeps);
                        outgoing[static_cast<std::size_t>(i)] = fractions.first;
                    }
                    auto moved =
                        MoistureDetail::substep(water, next_water, b, m.moisture, wind, outgoing, dt, report.moisture.transfers, work);
                    if (!moved)
                        fail(
                            moved.error().kind, moved.error().reason == MoistureReason::work ? WeatherReason::work : WeatherReason::numeric
                        );
                    water.swap(next_water);
                }
                WeatherTrajectory t{absolute + 1, 0, 0, 0, 0, 0, 0, 0, 0};
                for (std::size_t i = 0; i < water.size(); ++i) {
                    charge(work, 2);
                    t.mean_surface_kelvin += flow[i].surface_kelvin;
                    t.mean_reference_kelvin += water[i].reference_kelvin;
                    t.mean_east_m_s += flow[i].east_m_s;
                    t.mean_north_m_s += flow[i].north_m_s;
                    t.maximum_speed_m_s = std::max(t.maximum_speed_m_s, std::hypot(flow[i].east_m_s, flow[i].north_m_s));
                    t.potential_rms += flow[i].potential * flow[i].potential;
                    t.rain_export_m3 += report.moisture.transfers[i].rain_mm * unit;
                }
                const double count = static_cast<double>(r.cells);
                t.mean_surface_kelvin /= count;
                t.mean_reference_kelvin /= count;
                t.mean_east_m_s /= count;
                t.mean_north_m_s /= count;
                t.potential_rms = std::sqrt(t.potential_rms / count);
                t.water_m3 = water_total(water) * unit;
                report.trajectory.push_back(t);
            }
            charge(work, r.cells);
            finish(report.moisture, water, unit);
            return WeatherAdvance{
                WeatherState(
                    std::move(retained), std::move(water), std::move(flow), m, end, {work.used(), r.advance_storage_bytes, n, ticks * n},
                    std::move(force_east), std::move(force_north)
                ),
                std::move(report)
            };
        } catch (const Stop& s) {
            auto e = s.failure;
            e.work = work.used();
            return std::unexpected(e);
        }
    }
}
