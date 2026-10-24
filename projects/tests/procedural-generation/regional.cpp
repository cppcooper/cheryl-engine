#include "regional.h"
#include "weather-kernel.h"
#include "moisture-kernel.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace ProcGen::WeatherDetail {
    // Only operates on the unpublished regional candidate. U14 inputs/results
    // remain immutable. A failing update may alter scratch, which is discarded.
    struct RegionalAccess {
        static WeatherResult<void> evaporate(WeatherState& state, std::span<const double> amounts, Work& work) {
            for (std::size_t i = 0; i < amounts.size(); ++i) {
                if (!work.consume(2))
                    return std::unexpected(WeatherFailure{FailureKind::budget_refused, WeatherReason::work});
                state.water_[i].vapour_mm += amounts[i];
                if (!MoistureDetail::valid_cell(state.water_[i], state.baseline_, i, false))
                    return std::unexpected(WeatherFailure{FailureKind::unsupported, WeatherReason::numeric});
            }
            return {};
        }
    };
}
namespace ProcGen {
    namespace {
        struct Stop {
            RegionalFailure failure;
        };
        [[noreturn]] void fail(FailureKind kind, RegionalReason reason) {
            throw Stop{{kind, reason, 0, {}}};
        }
        [[noreturn]] void weather_fail(WeatherFailure error) {
            RegionalReason reason = RegionalReason::weather;
            switch (error.reason) {
                case WeatherReason::work:
                    reason = RegionalReason::work;
                    break;
                case WeatherReason::substeps:
                    reason = RegionalReason::substeps;
                    break;
                case WeatherReason::context:
                    reason = RegionalReason::source;
                    break;
                case WeatherReason::version:
                    reason = RegionalReason::version;
                    break;
                case WeatherReason::numeric:
                    reason = RegionalReason::numeric;
                    break;
                default:
                    break;
            }
            throw Stop{{error.kind, reason, 0, error}};
        }
        void charge(Work& work, Index n = 1) {
            if (!work.consume(n))
                fail(FailureKind::budget_refused, RegionalReason::work);
        }
        Index add(Index a, Index b) {
            if (a > std::numeric_limits<Index>::max() - b)
                fail(FailureKind::invalid_input, RegionalReason::overflow);
            return a + b;
        }
        Index mul(Index a, Index b) {
            if (b && a > std::numeric_limits<Index>::max() / b)
                fail(FailureKind::invalid_input, RegionalReason::overflow);
            return a * b;
        }
        bool range(double a, double lo, double hi) {
            return std::isfinite(a) && a >= lo && a <= hi;
        }
        RegionalRequirements needed(const WeatherState& state) {
            const auto q = weather_requirements(state.baseline().descriptor().domain);
            if (!q)
                weather_fail(q.error());
            const auto c = q->cells, f = q->faces, v = q->vertices;
            const auto& d = state.baseline().descriptor().domain;
            const auto w = d.extents()[0], h = d.extents()[1];
            const auto& m = state.model();
            const auto edges =
                (w - 1) * h + (h - 1) * w + (m.boundary == MoistureBoundary::periodic ? (w > 1 ? h : 0) + (h > 1 ? w : 0) : 0);
            // U14 model/numerics1's known one-tick, one-substep lower bound.
            const auto base = add(64, add(mul(12, c), add(mul(2, f), v)));
            const auto tick = add(add(mul(10, c), 1), add(mul(2, m.forcing.count), add(mul(v, add(1, mul(2, m.forcing.count))), f)));
            const auto step = add(mul(25, c), add(f, add(mul(2, edges), 2)));
            return {
                c,
                f,
                add(65536, add(mul(280, c), mul(8, f))),
                add(131072, add(mul(688, c), add(mul(24, f), mul(8, v)))),
                add(mul(35, c), f),
                add(mul(86, c), add(mul(3, f), v)),
                add(64, add(mul(9, c), f)),
                add(base, add(4, add(tick, step)))
            };
        }
        void admit(Index c, Index fields, Index bytes, Index work, RegionalBudgets b) {
            if (c > std::min(b.cells, regional_hard_limits.cells))
                fail(FailureKind::budget_refused, RegionalReason::cells);
            if (fields > std::min(b.field_values, regional_hard_limits.field_values))
                fail(FailureKind::budget_refused, RegionalReason::fields);
            if (bytes > std::min(b.storage_bytes, regional_hard_limits.storage_bytes))
                fail(FailureKind::budget_refused, RegionalReason::storage);
            if (work > std::min(b.work, regional_hard_limits.work))
                fail(FailureKind::budget_refused, RegionalReason::work);
        }
        void validate(
            const WeatherState& weather,
            std::span<const SoilCell> soil,
            const ClimateBaseline& expected,
            RegionalVersion version,
            Work& work
        ) {
            if (version != RegionalVersion{})
                fail(FailureKind::unsupported, RegionalReason::version);
            if (soil.size() != weather.water().size())
                fail(FailureKind::invalid_input, RegionalReason::shape);
            const auto source = WeatherDetail::check_source(weather.baseline(), expected, work);
            if (!source)
                weather_fail(source.error());
            for (const auto& a : soil) {
                charge(work, 4);
                if (!range(a.capacity_mm, 0, 1000) || !range(a.water_mm, 0, a.capacity_mm) || !range(a.infiltration_fraction, 0, 1) ||
                    !range(a.evaporation_mm_per_second, 0, .1))
                    fail(FailureKind::invalid_input, RegionalReason::soil);
            }
        }
        double total(const WeatherState& weather, std::span<const SoilCell> soil) {
            double sum = 0;
            for (std::size_t i = 0; i < soil.size(); ++i) {
                const auto& q = weather.water()[i];
                sum += q.vapour_mm + q.cloud_mm + q.finite_surface_mm + soil[i].water_mm;
            }
            return sum;
        }
        double unit(const WeatherState& w) {
            const auto& f = w.baseline().descriptor().context.frame;
            return f.dx_metres * f.dy_metres / 1000;
        }
        RegionalFailure stopped(const Stop& s, const Work& work) {
            auto e = s.failure;
            e.work = work.used();
            return e;
        }
        static_assert(sizeof(SoilCell) == 32 && sizeof(SoilTransfer) == 32 && sizeof(ObservedCell) == 336);
        static_assert(4 * sizeof(RegionalCheckpoint) + 4 * sizeof(RegionalState) + sizeof(RegionObservation) < 65536);
    }
    RegionalState::RegionalState(WeatherState weather, std::vector<SoilCell> soil, RegionalUsage usage)
    : weather_(std::move(weather)), soil_(std::move(soil)), usage_(usage) {}
    RegionalHistory::RegionalHistory(std::vector<RegionalCheckpoint> snapshots, RegionalUsage usage)
    : snapshots_(std::move(snapshots)), usage_(usage) {}
    RegionalResult<RegionalRequirements> regional_requirements(const WeatherState& state) {
        try {
            return needed(state);
        } catch (const Stop& s) {
            return std::unexpected(s.failure);
        }
    }
    RegionalResult<RegionalState> start_regional(const WeatherState& weather, std::span<const SoilCell> soil, RegionalBudgets budgets) {
        Work work(std::min(budgets.work, regional_hard_limits.work));
        try {
            charge(work, 64);
            const auto r = needed(weather);
            if (soil.size() != r.cells)
                fail(FailureKind::invalid_input, RegionalReason::shape);
            admit(r.cells, r.state_fields, r.state_storage_bytes, r.copy_work, budgets);
            validate(weather, soil, weather.baseline(), {}, work);
            charge(work, add(mul(4, r.cells), r.faces));
            return RegionalState(weather, {soil.begin(), soil.end()}, {work.used(), r.state_storage_bytes, r.state_fields});
        } catch (const Stop& s) {
            return std::unexpected(stopped(s, work));
        }
    }
    RegionalResult<RegionalCheckpoint> capture_regional(const RegionalState& state, RegionalBudgets budgets) {
        Work work(std::min(budgets.work, regional_hard_limits.work));
        try {
            charge(work, 64);
            if (!budgets.snapshots)
                fail(FailureKind::budget_refused, RegionalReason::history);
            const auto r = needed(state.weather());
            admit(r.cells, r.state_fields, r.state_storage_bytes, r.copy_work, budgets);
            validate(state.weather(), state.soil(), state.weather().baseline(), {}, work);
            charge(work, add(mul(4, r.cells), r.faces));
            return RegionalCheckpoint{state.weather(), {state.soil().begin(), state.soil().end()}, {}};
        } catch (const Stop& s) {
            return std::unexpected(stopped(s, work));
        }
    }
    RegionalResult<RegionalState>
    resume_regional(const RegionalCheckpoint& image, const ClimateBaseline& expected, RegionalBudgets budgets) {
        Work work(std::min(budgets.work, regional_hard_limits.work));
        try {
            charge(work, 64);
            if (!budgets.snapshots)
                fail(FailureKind::budget_refused, RegionalReason::history);
            const auto r = needed(image.weather);
            if (image.soil.size() != r.cells)
                fail(FailureKind::invalid_input, RegionalReason::shape);
            admit(r.cells, r.state_fields, r.state_storage_bytes, r.copy_work, budgets);
            validate(image.weather, image.soil, expected, image.version, work);
            charge(work, add(mul(4, r.cells), r.faces));
            return RegionalState(image.weather, image.soil, {work.used(), r.state_storage_bytes, r.state_fields});
        } catch (const Stop& s) {
            return std::unexpected(stopped(s, work));
        }
    }
    RegionalResult<RegionalHistory>
    retain_regional_history(std::span<const RegionalCheckpoint> images, const ClimateBaseline& expected, RegionalBudgets budgets) {
        Work work(std::min(budgets.work, regional_hard_limits.work));
        try {
            charge(work, 64);
            if (images.empty())
                fail(FailureKind::invalid_input, RegionalReason::history);
            if (images.size() > std::min(budgets.snapshots, regional_hard_limits.snapshots))
                fail(FailureKind::budget_refused, RegionalReason::history);
            Index bytes = 0, fields = 0, minimum = 64;
            for (const auto& image : images) {
                const auto r = needed(image.weather);
                if (r.cells > std::min(budgets.cells, regional_hard_limits.cells))
                    fail(FailureKind::budget_refused, RegionalReason::cells);
                bytes = add(bytes, r.state_storage_bytes);
                fields = add(fields, r.state_fields);
                minimum = add(minimum, r.copy_work);
            }
            admit(0, fields, bytes, minimum, budgets);
            for (std::size_t i = 0; i < images.size(); ++i) {
                charge(work, 64);
                if (i && images[i - 1].weather.tick() >= images[i].weather.tick())
                    fail(FailureKind::invalid_input, RegionalReason::history);
                validate(images[i].weather, images[i].soil, expected, images[i].version, work);
            }
            std::vector<RegionalCheckpoint> snapshots;
            snapshots.reserve(images.size());
            for (const auto& image : images) {
                const auto r = needed(image.weather);
                charge(work, add(mul(4, r.cells), r.faces));
                snapshots.push_back(image);
            }
            return RegionalHistory(std::move(snapshots), {work.used(), bytes, fields});
        } catch (const Stop& s) {
            return std::unexpected(stopped(s, work));
        }
    }
    RegionalResult<RegionalAdvance> advance_regional(const RegionalState& old, Index ticks, RegionalBudgets budgets) {
        Work work(std::min(budgets.work, regional_hard_limits.work));
        try {
            charge(work, 64);
            if (!ticks)
                fail(FailureKind::invalid_input, RegionalReason::ticks);
            if (ticks > std::min(budgets.ticks, regional_hard_limits.ticks))
                fail(FailureKind::budget_refused, RegionalReason::ticks);
            const auto r = needed(old.weather());
            const auto end = add(old.weather().tick(), ticks);
            const auto minimum = add(add(64, add(mul(8, r.cells), r.faces)), mul(ticks, add(1, add(mul(8, r.cells), r.minimum_tick_work))));
            admit(r.cells, r.advance_fields, r.advance_storage_bytes, minimum, budgets);
            charge(work, add(mul(6, r.cells), r.faces));
            auto weather = old.weather();
            std::vector<SoilCell> soil(old.soil().begin(), old.soil().end());
            std::vector<double> evaporation(soil.size());
            RegionalReport report{
                old.weather().tick(),
                end,
                weather.baseline().descriptor().context.calendar.seconds_per_tick,
                std::vector<SoilTransfer>(soil.size()),
                {}
            };
            const auto volume = unit(weather);
            charge(work, r.cells);
            report.water.initial_m3 = total(weather, soil) * volume;
            for (Index tick = 0; tick < ticks; ++tick) {
                charge(work);
                const MoistureBudgets child{
                    budgets.cells, std::min(budgets.storage_bytes, weather_hard_limits.storage_bytes),
                    std::min(budgets.work, regional_hard_limits.work) - work.used(), 1,
                    std::min(budgets.substeps_per_tick, regional_hard_limits.substeps_per_tick)
                };
                auto advanced = advance_weather(weather, weather.baseline(), 1, child);
                if (!advanced) {
                    charge(work, advanced.error().work);
                    weather_fail(advanced.error());
                }
                charge(work, advanced->state.usage().work);
                report.water.replenished_m3 += advanced->report.moisture.water.replenished_m3;
                report.water.finite_evaporation_m3 += advanced->report.moisture.water.finite_evaporation_m3;
                for (std::size_t i = 0; i < soil.size(); ++i) {
                    charge(work, 6);
                    const double rain = advanced->report.moisture.transfers[i].rain_mm;
                    auto& s = soil[i];
                    auto& t = report.transfers[i];
                    const double infiltrated = std::min(s.capacity_mm - s.water_mm, rain * s.infiltration_fraction);
                    const double overflow = rain - infiltrated;
                    s.water_mm += infiltrated;
                    const double evaporated =
                        std::min(s.water_mm, s.evaporation_mm_per_second * static_cast<double>(report.seconds_per_tick));
                    s.water_mm -= evaporated;
                    evaporation[i] = evaporated;
                    t.precipitation_mm += rain;
                    t.infiltration_mm += infiltrated;
                    t.overflow_mm += overflow;
                    t.evaporation_mm += evaporated;
                    report.water.precipitation_m3 += rain * volume;
                    report.water.infiltration_m3 += infiltrated * volume;
                    report.water.overflow_export_m3 += overflow * volume;
                    report.water.soil_evaporation_m3 += evaporated * volume;
                    if (!range(s.water_mm, 0, s.capacity_mm))
                        fail(FailureKind::unsupported, RegionalReason::numeric);
                }
                auto applied = WeatherDetail::RegionalAccess::evaporate(advanced->state, evaporation, work);
                if (!applied)
                    weather_fail(applied.error());
                weather = std::move(advanced->state);
            }
            charge(work, r.cells);
            auto& l = report.water;
            l.final_m3 = total(weather, soil) * volume;
            l.residual_m3 = l.final_m3 + l.overflow_export_m3 - l.initial_m3 - l.replenished_m3;
            l.tolerance_m3 = 1e-10 * std::max(1.0, l.initial_m3 + l.replenished_m3 + l.overflow_export_m3);
            if (!std::isfinite(l.residual_m3) || std::abs(l.residual_m3) > l.tolerance_m3)
                fail(FailureKind::unsupported, RegionalReason::numeric);
            return RegionalAdvance{
                RegionalState(std::move(weather), std::move(soil), {work.used(), r.advance_storage_bytes, r.advance_fields}),
                std::move(report)
            };
        } catch (const Stop& s) {
            return std::unexpected(stopped(s, work));
        }
    }
    RegionalResult<RegionObservation> observe_region(const RegionalState& state, RegionWindow window, RegionalBudgets budgets) {
        Work work(std::min(budgets.work, regional_hard_limits.work));
        try {
            charge(work, 64);
            const auto& source = state.weather().baseline().descriptor();
            const auto w = source.domain.extents()[0], h = source.domain.extents()[1];
            if (!window.width || !window.height || add(window.x, window.width) > w || add(window.y, window.height) > h ||
                window.frame != source.context.frame)
                fail(FailureKind::invalid_input, RegionalReason::region);
            const auto c = mul(window.width, window.height), fields = mul(42, c), bytes = add(32768, mul(336, c)),
                       needed_work = add(64, mul(8, c));
            admit(c, fields, bytes, needed_work, budgets);
            RegionObservation out{window, source, state.weather().model(), {}, state.weather().tick(), {}, {0, bytes, fields}};
            out.cells.reserve(static_cast<std::size_t>(c));
            for (Index y = window.y; y < window.y + window.height; ++y)
                for (Index x = window.x; x < window.x + window.width; ++x) {
                    charge(work, 8);
                    const auto index = x + y * w;
                    const auto i = static_cast<std::size_t>(index);
                    const auto q = state.weather().water()[i];
                    const auto f = state.weather().flow()[i];
                    const auto soil = state.soil()[i];
                    const double local = MoistureDetail::local_temperature(q.reference_kelvin, state.weather().baseline(), i);
                    const double capacity = MoistureDetail::capacity(local, state.weather().model().moisture);
                    const double saturation = capacity > 0 ? std::clamp(q.vapour_mm / capacity, 0.0, 1.0) : (q.vapour_mm > 0 ? 1 : 0);
                    const double fog = q.cloud_mm / (1 + q.cloud_mm), dry = soil.capacity_mm > 0 ? 1 - soil.water_mm / soil.capacity_mm : 1;
                    const double wind = std::hypot(f.east_m_s, f.north_m_s), fire = dry * wind / (1 + wind);
                    out.cells.push_back(
                        {index, state.weather().baseline().cells()[i], q, f, soil, {local, saturation, fog, dry, wind, fire}}
                    );
                }
            out.usage.work = work.used();
            return out;
        } catch (const Stop& s) {
            return std::unexpected(stopped(s, work));
        }
    }
}
