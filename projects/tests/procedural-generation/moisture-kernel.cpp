#include "moisture-kernel.h"
#include <algorithm>
#include <cmath>

namespace ProcGen::MoistureDetail {
    namespace {
        struct Stop {
            MoistureFailure failure;
        };
        [[noreturn]] void fail(FailureKind kind, MoistureReason reason) {
            throw Stop{{kind, reason}};
        }
        void charge(Work& work, Index amount = 1) {
            if (!work.consume(amount))
                fail(FailureKind::budget_refused, MoistureReason::work);
        }
        bool range(double x, double a, double b) {
            return std::isfinite(x) && x >= a && x <= b;
        }
    }
    double capacity(double t, const MoistureModel& m) {
        return std::clamp(m.capacity_at_273_mm + m.capacity_mm_per_kelvin * (t - 273.15), m.minimum_capacity_mm, m.maximum_capacity_mm);
    }
    double local_temperature(double reference, const ClimateBaseline& b, std::size_t i) {
        const auto& m = b.descriptor().context.model;
        return reference - m.lapse_kelvin_per_metre * (b.cells()[i].z_metres - m.reference_metres);
    }
    bool valid_cell(const MoistureCell& a, const ClimateBaseline& b, std::size_t i, bool initial) {
        return range(a.vapour_mm, 0, 1e12) && range(a.cloud_mm, 0, 1e12) && range(a.finite_surface_mm, 0, 1e12) &&
               range(a.reference_kelvin, initial ? 250 : 249, initial ? 600 : 601) &&
               range(local_temperature(a.reference_kelvin, b, i), 1, 2000) &&
               (a.finite_surface_mm == 0 || b.cells()[i].surface.finite_water_fraction > 0);
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
        const bool skip_x = w == 1 && f.boundary == MoistureBoundary::periodic, skip_y = h == 1 && f.boundary == MoistureBoundary::periodic;
        const double left = skip_x ? 0 : f.east[static_cast<std::size_t>(x + y * (w + 1))];
        const double right = skip_x ? 0 : f.east[static_cast<std::size_t>(x + 1 + y * (w + 1))];
        const double top = skip_y ? 0 : sign * f.north[static_cast<std::size_t>(x + y * w)];
        const double bottom = skip_y ? 0 : sign * f.north[static_cast<std::size_t>(x + (y + 1) * w)];
        const auto fx = [&](double v) { return std::max(0.0, v) * dt / frame.dx_metres; };
        const auto fy = [&](double v) { return std::max(0.0, v) * dt / frame.dy_metres; };
        return {fx(-left) + fx(right) + fy(-top) + fy(bottom), fx(left) + fx(-right) + fy(top) + fy(-bottom)};
    }

    MoistureResult<void> substep(
        std::span<const MoistureCell> cells,
        std::span<MoistureCell> next,
        const ClimateBaseline& baseline,
        const MoistureModel& model,
        FaceWinds owned,
        std::span<const double> outgoing,
        double dt,
        std::span<MoistureTransfer> transfers,
        Work& work
    ) {
        try {
            const auto& d = baseline.descriptor().domain;
            const auto& frame = baseline.descriptor().context.frame;
            const double sign = frame.north == RowNorth::increasing ? 1 : -1;
            const double cond_fraction = std::min(1.0, model.condensation_per_second * dt),
                         cloud_evap_fraction = std::min(1.0, model.cloud_evaporation_per_second * dt),
                         rain_fraction = std::min(1.0, model.rain_per_second * dt);
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
                const auto from = static_cast<std::size_t>(velocity > 0 ? a : b), to = static_cast<std::size_t>(velocity > 0 ? b : a);
                const double fraction = std::abs(velocity) * dt / (horizontal ? frame.dx_metres : frame.dy_metres);
                next[to].vapour_mm += cells[from].vapour_mm * fraction;
                next[to].cloud_mm += cells[from].cloud_mm * fraction;
                next[to].reference_kelvin += fraction * (cells[from].reference_kelvin - cells[to].reference_kelvin);
            });
            for (std::size_t i = 0; i < cells.size(); ++i) {
                charge(work, 6);
                auto& cell = next[i];
                auto& amounts = transfers[i];
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
                if (!valid_cell(cell, baseline, i, false))
                    fail(FailureKind::unsupported, MoistureReason::numeric);
            }

            charge(work); // caller swaps its two buffers after successful completion
            return {};
        } catch (const Stop& s) {
            return std::unexpected(s.failure);
        }
    }
}
