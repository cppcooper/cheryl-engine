#include "moisture.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace ProcGen {
    namespace {
        struct Stop {
            MoistureFailure failure;
        };
        [[noreturn]] void fail(FailureKind kind, MoistureReason reason) {
            throw Stop{{kind, reason}};
        }
        Index add(Index a, Index b) {
            if (a > std::numeric_limits<Index>::max() - b)
                fail(FailureKind::invalid_input, MoistureReason::overflow);
            return a + b;
        }
        Index mul(Index a, Index b) {
            if (b != 0 && a > std::numeric_limits<Index>::max() / b)
                fail(FailureKind::invalid_input, MoistureReason::overflow);
            return a * b;
        }
        void charge(Work& w, Index n = 1) {
            if (!w.consume(n))
                fail(FailureKind::budget_refused, MoistureReason::work);
        }
        bool range(double x, double a, double b) {
            return std::isfinite(x) && x >= a && x <= b;
        }
        void model_ok(const MoistureModel& m) {
            if (m.version != 1 || m.numerics_version != 1)
                fail(FailureKind::unsupported, MoistureReason::version);
            if (!range(m.capacity_at_273_mm, 0, 1000) || !range(m.capacity_mm_per_kelvin, 0, 10) ||
                !range(m.minimum_capacity_mm, 0, 1000) || !range(m.maximum_capacity_mm, m.minimum_capacity_mm, 1000) ||
                !range(m.condensation_per_second, 0, 1) || !range(m.cloud_evaporation_per_second, 0, 1) ||
                !range(m.rain_per_second, 0, 1) || !range(m.surface_evaporation_mm_per_second, 0, .1))
                fail(FailureKind::invalid_input, MoistureReason::model);
        }
        double capacity(double t, const MoistureModel& m) {
            return std::clamp(m.capacity_at_273_mm + m.capacity_mm_per_kelvin * (t - 273.15), m.minimum_capacity_mm, m.maximum_capacity_mm);
        }
        double local_temperature(double reference, const ClimateBaseline& b, std::size_t i) {
            const auto& m = b.descriptor().context.model;
            return reference - m.lapse_kelvin_per_metre * (b.cells()[i].z_metres - m.reference_metres);
        }
        bool cell_ok(const MoistureCell& a, const ClimateBaseline& b, std::size_t i, bool initial) {
            return range(a.vapour_mm, 0, 1e12) && range(a.cloud_mm, 0, 1e12) && range(a.finite_surface_mm, 0, 1e12) &&
                   range(a.reference_kelvin, initial ? 250 : 249, initial ? 600 : 601) &&
                   range(local_temperature(a.reference_kelvin, b, i), 1, 2000) &&
                   (a.finite_surface_mm == 0 || b.cells()[i].surface.finite_water_fraction > 0);
        }
        MoistureRequirements needed(const Domain& d) {
            if (d.dimensions() != 2)
                fail(FailureKind::invalid_input, MoistureReason::dimensions);
            const auto c = d.cells(), w = d.extents()[0], h = d.extents()[1];
            const auto ef = mul(add(w, 1), h), nf = mul(w, add(h, 1));
            const auto faces = add(ef, nf), inner = add(mul(w - 1, h), mul(h - 1, w));
            const auto start = add(moisture_fixed_storage, mul(216, c));
            const auto advance = add(moisture_fixed_storage, add(mul(304, c), mul(8, faces)));
            if (c > std::vector<MoistureCell>{}.max_size() || c > std::vector<ClimateCell>{}.max_size() ||
                c > std::vector<MoistureTransfer>{}.max_size() || ef > std::vector<double>{}.max_size() ||
                nf > std::vector<double>{}.max_size())
                fail(FailureKind::invalid_input, MoistureReason::overflow);
            return {c, ef, nf, inner, start, advance, add(64, mul(8, c))};
        }
        void admit(const MoistureRequirements& r, MoistureBudgets b, bool advance, Index minimum_work, const Domain& d) {
            if (r.cells > std::min(b.cells, moisture_hard_limits.cells) || d.extents()[0] > 128 || d.extents()[1] > 128)
                fail(FailureKind::budget_refused, MoistureReason::cells);
            if ((advance ? r.advance_storage_bytes : r.start_storage_bytes) > std::min(b.storage_bytes, moisture_hard_limits.storage_bytes))
                fail(FailureKind::budget_refused, MoistureReason::storage);
            if (minimum_work > std::min(b.work, moisture_hard_limits.work))
                fail(FailureKind::budget_refused, MoistureReason::work);
        }
        bool same_elevation(const std::optional<ElevationReplay>& a, const std::optional<ElevationReplay>& b) {
            if (a.has_value() != b.has_value())
                return false;
            return !a || (a->domain == b->domain && a->random == b->random && a->requested == b->requested &&
                          a->algorithm == b->algorithm && a->version == b->version);
        }
        void match(const ClimateBaseline& a, const ClimateBaseline& b, Work& work) {
            const auto& x = a.descriptor();
            const auto& y = b.descriptor();
            if (x.domain != y.domain || x.revision != y.revision || x.context != y.context || x.tick != y.tick ||
                !same_elevation(x.elevation, y.elevation) || a.cells().size() != b.cells().size())
                fail(FailureKind::invalid_input, MoistureReason::context);
            for (std::size_t i = 0; i < a.cells().size(); ++i) {
                charge(work);
                if (a.cells()[i] != b.cells()[i])
                    fail(FailureKind::invalid_input, MoistureReason::context);
            }
        }
        void winds_ok(FaceWinds f, const Domain& d, Work& work) {
            const auto w = d.extents()[0], h = d.extents()[1];
            for (Index y = 0; y < h; ++y)
                for (Index x = 0; x <= w; ++x) {
                    charge(work);
                    const double u = f.east[static_cast<std::size_t>(x + y * (w + 1))];
                    if (!range(u, -1000, 1000))
                        fail(FailureKind::invalid_input, MoistureReason::wind);
                    if (f.boundary == MoistureBoundary::closed && (x == 0 || x == w) && u != 0)
                        fail(FailureKind::invalid_input, MoistureReason::boundary);
                    if (f.boundary == MoistureBoundary::periodic && x == 0 && u != f.east[static_cast<std::size_t>(w + y * (w + 1))])
                        fail(FailureKind::invalid_input, MoistureReason::boundary);
                }
            for (Index y = 0; y <= h; ++y)
                for (Index x = 0; x < w; ++x) {
                    charge(work);
                    const double v = f.north[static_cast<std::size_t>(x + y * w)];
                    if (!range(v, -1000, 1000))
                        fail(FailureKind::invalid_input, MoistureReason::wind);
                    if (f.boundary == MoistureBoundary::closed && (y == 0 || y == h) && v != 0)
                        fail(FailureKind::invalid_input, MoistureReason::boundary);
                    if (f.boundary == MoistureBoundary::periodic && y == 0 && v != f.north[static_cast<std::size_t>(x + h * w)])
                        fail(FailureKind::invalid_input, MoistureReason::boundary);
                }
        }
        // Canonical face order: east row-major interior then wrap per row; north
        // row-major interior then final wrap row. No ghost values or duplicated seam.
        template <class F> void faces(FaceWinds f, const Domain& d, double sign, F&& visit) {
            const auto w = d.extents()[0], h = d.extents()[1];
            for (Index y = 0; y < h; ++y) {
                for (Index x = 1; x < w; ++x)
                    visit(x - 1 + y * w, x + y * w, f.east[static_cast<std::size_t>(x + y * (w + 1))], true);
                if (w > 1 && f.boundary == MoistureBoundary::periodic)
                    visit(w - 1 + y * w, y * w, f.east[static_cast<std::size_t>(y * (w + 1))], true);
            }
            for (Index y = 1; y < h; ++y)
                for (Index x = 0; x < w; ++x)
                    visit(x + (y - 1) * w, x + y * w, sign * f.north[static_cast<std::size_t>(x + y * w)], false);
            if (h > 1 && f.boundary == MoistureBoundary::periodic)
                for (Index x = 0; x < w; ++x)
                    visit(x + (h - 1) * w, x, sign * f.north[static_cast<std::size_t>(x)], false);
        }
        std::pair<double, double> fractions(FaceWinds f, const Domain& d, const RegionalFrame& frame, Index i, double dt) {
            const auto w = d.extents()[0], h = d.extents()[1], x = i % w, y = i / w;
            const double sign = frame.north == RowNorth::increasing ? 1 : -1;
            const bool skip_x = w == 1 && f.boundary == MoistureBoundary::periodic,
                       skip_y = h == 1 && f.boundary == MoistureBoundary::periodic;
            const double left = skip_x ? 0 : f.east[static_cast<std::size_t>(x + y * (w + 1))];
            const double right = skip_x ? 0 : f.east[static_cast<std::size_t>(x + 1 + y * (w + 1))];
            const double top = skip_y ? 0 : sign * f.north[static_cast<std::size_t>(x + y * w)];
            const double bottom = skip_y ? 0 : sign * f.north[static_cast<std::size_t>(x + (y + 1) * w)];
            const auto fx = [&](double v) { return std::max(0.0, v) * dt / frame.dx_metres; };
            const auto fy = [&](double v) { return std::max(0.0, v) * dt / frame.dy_metres; };
            return {fx(-left) + fx(right) + fy(-top) + fy(bottom), fx(left) + fx(-right) + fy(top) + fy(-bottom)};
        }
        Index canonical_count(const MoistureRequirements& r, const Domain& d, MoistureBoundary b) {
            return r.interior_faces + (b == MoistureBoundary::periodic
                                           ? (d.extents()[0] > 1 ? d.extents()[1] : 0) + (d.extents()[1] > 1 ? d.extents()[0] : 0)
                                           : 0);
        }
        double total_mm(std::span<const MoistureCell> cells) {
            double total = 0;
            for (const auto& c : cells)
                total += c.vapour_mm + c.cloud_mm + c.finite_surface_mm;
            return total;
        }
        static_assert(sizeof(MoistureCell) == 32 && sizeof(MoistureTransfer) == 40 && sizeof(ClimateCell) == 184);
        static_assert(3 * sizeof(MoistureState) + 2 * sizeof(ClimateDescriptor) + 4096 <= moisture_fixed_storage);
    }
    std::string_view moisture_reason_text(MoistureReason r) {
        switch (r) {
            case MoistureReason::dimensions:
                return "moisture requires a finite checked2D domain";
            case MoistureReason::shape:
                return "moisture cell/face array size mismatch";
            case MoistureReason::context:
                return "incompatible exact climate/source/frame context";
            case MoistureReason::initial:
                return "invalid initial water/temperature/finite reservoir";
            case MoistureReason::model:
                return "invalid capacity or transfer model parameters";
            case MoistureReason::wind:
                return "nonfinite or unsupported face wind";
            case MoistureReason::boundary:
                return "invalid/unsupported closed or periodic boundary faces";
            case MoistureReason::overflow:
                return "moisture index/clock/storage/work overflow";
            case MoistureReason::cells:
                return "moisture cell/axis budget refused";
            case MoistureReason::storage:
                return "moisture accounted storage refused";
            case MoistureReason::work:
                return "moisture work refused or exhausted";
            case MoistureReason::ticks:
                return "moisture requires1..10 admitted whole ticks";
            case MoistureReason::substeps:
                return "actual face fractions require unsupported substeps";
            case MoistureReason::numeric:
                return "moisture numerical range or water residual refused";
            case MoistureReason::version:
                return "unsupported moisture model/numerics version";
        }
        return "unknown moisture failure";
    }
    MoistureResult<MoistureRequirements> moisture_requirements(const Domain& d) {
        try {
            return needed(d);
        } catch (const Stop& s) {
            return std::unexpected(s.failure);
        }
    }
    MoistureResult<double> moisture_capacity(double t, const MoistureModel& m) {
        try {
            model_ok(m);
            if (!range(t, 1, 2000))
                fail(FailureKind::invalid_input, MoistureReason::initial);
            return capacity(t, m);
        } catch (const Stop& s) {
            return std::unexpected(s.failure);
        }
    }
    MoistureState::MoistureState(
        ClimateBaseline baseline,
        std::vector<MoistureCell> cells,
        MoistureModel model,
        Index tick,
        std::vector<double> east,
        std::vector<double> north,
        std::optional<MoistureWindInterval> interval,
        MoistureUsage usage
    )
    : baseline_(std::move(baseline)),
      cells_(std::move(cells)),
      model_(model),
      tick_(tick),
      east_(std::move(east)),
      north_(std::move(north)),
      wind_(interval),
      usage_(usage) {}
    MoistureResult<MoistureState>
    start_moisture(const ClimateBaseline& baseline, std::span<const MoistureCell> initial, MoistureModel model, MoistureBudgets budget) {
        Work work(std::min(budget.work, moisture_hard_limits.work));
        try {
            const auto& d = baseline.descriptor().domain;
            const auto r = needed(d);
            if (initial.size() != r.cells)
                fail(FailureKind::invalid_input, MoistureReason::shape);
            admit(r, budget, false, r.start_work, d);
            charge(work, 64);
            model_ok(model);
            for (std::size_t i = 0; i < initial.size(); ++i) {
                charge(work, 2);
                if (!cell_ok(initial[i], baseline, i, true))
                    fail(FailureKind::invalid_input, MoistureReason::initial);
            }
            charge(work, mul(6, r.cells));
            return MoistureState(
                baseline, {initial.begin(), initial.end()}, model, baseline.descriptor().tick, {}, {}, std::nullopt,
                {work.used(), r.start_storage_bytes, 0, 0, 0}
            );
        } catch (const Stop& s) {
            auto e = s.failure;
            e.work = work.used();
            return std::unexpected(e);
        }
    }
    MoistureResult<MoistureAdvance>
    advance_moisture(const MoistureState& old, const ClimateBaseline& expected, FaceWinds wind, Index ticks, MoistureBudgets budget) {
        Work work(std::min(budget.work, moisture_hard_limits.work));
        try {
            const auto& baseline = old.baseline();
            const auto& d = baseline.descriptor().domain;
            const auto r = needed(d);
            if (ticks == 0)
                fail(FailureKind::invalid_input, MoistureReason::ticks);
            if (ticks > std::min(budget.ticks, moisture_hard_limits.ticks))
                fail(FailureKind::budget_refused, MoistureReason::ticks);
            const auto to_tick = add(old.tick(), ticks);
            if (wind.boundary != MoistureBoundary::closed && wind.boundary != MoistureBoundary::periodic)
                fail(FailureKind::unsupported, MoistureReason::boundary);
            if (wind.east.size() != r.east_faces || wind.north.size() != r.north_faces)
                fail(FailureKind::invalid_input, MoistureReason::shape);
            const auto f = add(r.east_faces, r.north_faces), canonical = canonical_count(r, d, wind.boundary);
            const auto step_work = add(mul(7, r.cells), add(mul(2, canonical), 1));
            const auto base_work = add(64, add(mul(9, r.cells), mul(2, f)));
            const auto minimum = add(base_work, add(add(mul(4, r.cells), 1), add(ticks, mul(ticks, step_work))));
            admit(r, budget, true, minimum, d);
            const auto max_steps = std::min(budget.substeps_per_tick, moisture_hard_limits.substeps_per_tick);
            if (max_steps == 0)
                fail(FailureKind::budget_refused, MoistureReason::substeps);
            charge(work, 64);
            match(baseline, expected, work);
            winds_ok(wind, d, work);
            charge(work, mul(2, r.cells));
            std::vector<double> outgoing(static_cast<std::size_t>(r.cells)), incoming(static_cast<std::size_t>(r.cells));
            Index n = 0;
            double dt = 0;
            const auto& frame = baseline.descriptor().context.frame;
            const double seconds = static_cast<double>(baseline.descriptor().context.calendar.seconds_per_tick);
            for (Index trial = 1; trial <= max_steps; ++trial) {
                charge(work);
                dt = seconds / static_cast<double>(trial);
                bool allowed = true;
                for (Index i = 0; i < r.cells; ++i) {
                    charge(work, 4);
                    const auto v = fractions(wind, d, frame, i, dt);
                    outgoing[static_cast<std::size_t>(i)] = v.first;
                    incoming[static_cast<std::size_t>(i)] = v.second;
                    if (v.first > 1 || v.second > 1)
                        allowed = false;
                }
                if (allowed) {
                    n = trial;
                    break;
                }
            }
            if (n == 0)
                fail(FailureKind::budget_refused, MoistureReason::substeps);
            const auto steps = mul(ticks, n);
            const auto exact = add(base_work, add(mul(n, add(mul(4, r.cells), 1)), add(ticks, mul(steps, step_work))));
            if (exact > std::min(budget.work, moisture_hard_limits.work))
                fail(FailureKind::budget_refused, MoistureReason::work);
            charge(work, add(mul(5, r.cells), f));
            ClimateBaseline retained = baseline;
            std::vector<MoistureCell> cells(old.cells().begin(), old.cells().end()), next(static_cast<std::size_t>(r.cells));
            std::vector<double> east(wind.east.begin(), wind.east.end()), north(wind.north.begin(), wind.north.end());
            const FaceWinds owned{east, north, wind.boundary};
            MoistureReport report{
                old.tick(),
                to_tick,
                baseline.descriptor().context.calendar.seconds_per_tick,
                std::vector<MoistureTransfer>(static_cast<std::size_t>(r.cells)),
                {}
            };
            const double unit_volume = frame.dx_metres * frame.dy_metres / 1000;
            report.water.initial_m3 = total_mm(cells) * unit_volume;
            const auto& model = old.model();
            const double cond_fraction = std::min(1.0, model.condensation_per_second * dt),
                         cloud_evap_fraction = std::min(1.0, model.cloud_evaporation_per_second * dt),
                         rain_fraction = std::min(1.0, model.rain_per_second * dt);
            const double sign = frame.north == RowNorth::increasing ? 1 : -1;
            for (Index tick = 0; tick < ticks; ++tick) {
                charge(work);
                for (Index sub = 0; sub < n; ++sub) {
                    for (std::size_t i = 0; i < cells.size(); ++i) {
                        charge(work);
                        next[i] = cells[i];
                        next[i].vapour_mm *= 1 - outgoing[i];
                        next[i].cloud_mm *= 1 - outgoing[i];
                    }
                    faces(owned, d, sign, [&](Index a, Index b, double velocity, bool horizontal) {
                        charge(work, 2);
                        if (velocity == 0)
                            return;
                        const auto from = static_cast<std::size_t>(velocity > 0 ? a : b),
                                   to = static_cast<std::size_t>(velocity > 0 ? b : a);
                        const double fraction = std::abs(velocity) * dt / (horizontal ? frame.dx_metres : frame.dy_metres);
                        next[to].vapour_mm += cells[from].vapour_mm * fraction;
                        next[to].cloud_mm += cells[from].cloud_mm * fraction;
                        next[to].reference_kelvin += fraction * (cells[from].reference_kelvin - cells[to].reference_kelvin);
                    });
                    for (std::size_t i = 0; i < cells.size(); ++i) {
                        charge(work, 6);
                        auto& cell = next[i];
                        auto& amounts = report.transfers[i];
                        const auto& surface = baseline.cells()[i].surface;
                        const double finite =
                            std::min(cell.finite_surface_mm, model.surface_evaporation_mm_per_second * surface.finite_water_fraction * dt);
                        const double replenished = model.surface_evaporation_mm_per_second * surface.replenished_water_fraction * dt;
                        cell.finite_surface_mm -= finite;
                        cell.vapour_mm += finite + replenished;
                        amounts.finite_evaporation_mm += finite;
                        amounts.replenished_mm += replenished;
                        const double cap = capacity(local_temperature(cell.reference_kelvin, baseline, i), model);
                        if (cell.vapour_mm > cap) {
                            const double amount = (cell.vapour_mm - cap) * cond_fraction;
                            cell.vapour_mm -= amount;
                            cell.cloud_mm += amount;
                            amounts.condensed_mm += amount;
                        } else {
                            const double amount = std::min(cap - cell.vapour_mm, cell.cloud_mm) * cloud_evap_fraction;
                            cell.cloud_mm -= amount;
                            cell.vapour_mm += amount;
                            amounts.cloud_evaporated_mm += amount;
                        }
                        const double rain = cell.cloud_mm * rain_fraction;
                        cell.cloud_mm -= rain;
                        amounts.rain_mm += rain;
                        if (!cell_ok(cell, baseline, i, false))
                            fail(FailureKind::unsupported, MoistureReason::numeric);
                    }
                    charge(work);
                    cells.swap(next);
                }
            }
            charge(work, r.cells);
            report.water.final_m3 = total_mm(cells) * unit_volume;
            for (const auto& t : report.transfers) {
                report.water.finite_evaporation_m3 += t.finite_evaporation_mm * unit_volume;
                report.water.replenished_m3 += t.replenished_mm * unit_volume;
                report.water.condensed_m3 += t.condensed_mm * unit_volume;
                report.water.cloud_evaporated_m3 += t.cloud_evaporated_mm * unit_volume;
                report.water.rain_export_m3 += t.rain_mm * unit_volume;
            }
            auto& ledger = report.water;
            ledger.residual_m3 = ledger.final_m3 + ledger.rain_export_m3 - ledger.initial_m3 - ledger.replenished_m3;
            ledger.tolerance_m3 = 1e-10 * std::max(1.0, ledger.initial_m3 + ledger.replenished_m3 + ledger.rain_export_m3);
            if (!std::isfinite(ledger.residual_m3) || std::abs(ledger.residual_m3) > ledger.tolerance_m3)
                fail(FailureKind::unsupported, MoistureReason::numeric);
            MoistureState state(
                std::move(retained), std::move(cells), model, to_tick, std::move(east), std::move(north),
                MoistureWindInterval{wind.boundary, old.tick(), ticks}, {work.used(), r.advance_storage_bytes, n, steps, n}
            );
            return MoistureAdvance{std::move(state), std::move(report)};
        } catch (const Stop& s) {
            auto e = s.failure;
            e.work = work.used();
            return std::unexpected(e);
        }
    }
}
