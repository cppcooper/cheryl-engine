#include "weather-kernel.h"
#include <cmath>

namespace ProcGen::WeatherDetail {
    WeatherResult<void> wave(
        std::span<const FlowCell> old,
        std::span<FlowCell> next,
        const Domain& d,
        const RegionalFrame& f,
        double c,
        double dt,
        MoistureBoundary boundary,
        Work& work
    ) {
        const auto w = d.extents()[0], h = d.extents()[1];
        const bool periodic = boundary == MoistureBoundary::periodic;
        const double ix = periodic && w == 1 ? 0 : 1 / f.dx_metres, iy = periodic && h == 1 ? 0 : 1 / f.dy_metres;
        if (c < 0 || !std::isfinite(c) || !std::isfinite(dt) || dt < 0 || c * dt * (ix + iy) > .5)
            return std::unexpected(WeatherFailure{FailureKind::unsupported, WeatherReason::substeps});
        const auto neighbor = [&](Index x, Index y, int axis, int side) {
            auto value = old[static_cast<std::size_t>(x + y * w)];
            const bool edge = axis == 0 ? (side < 0 ? x == 0 : x + 1 == w) : (side < 0 ? y == 0 : y + 1 == h);
            if (edge && !periodic) {
                if (axis == 0)
                    value.east_m_s = -value.east_m_s;
                else
                    value.north_m_s = -value.north_m_s;
                return value;
            }
            if (axis == 0)
                x = side < 0 ? (x == 0 ? w - 1 : x - 1) : (x + 1 == w ? 0 : x + 1);
            else
                y = side < 0 ? (y == 0 ? h - 1 : y - 1) : (y + 1 == h ? 0 : y + 1);
            return old[static_cast<std::size_t>(x + y * w)];
        };
        const double sign = f.north == RowNorth::increasing ? 1 : -1;
        for (Index i = 0; i < d.cells(); ++i) {
            if (!work.consume(6))
                return std::unexpected(WeatherFailure{FailureKind::budget_refused, WeatherReason::work});
            const auto a = old[static_cast<std::size_t>(i)];
            auto result = a;
            if (c != 0) {
                const Index x = i % w, y = i / w;
                const auto l = neighbor(x, y, 0, -1), r = neighbor(x, y, 0, 1), t = neighbor(x, y, 1, -1), b = neighbor(x, y, 1, 1);
                const double lx = c * dt / f.dx_metres, ly = c * dt / f.dy_metres;
                const double q = a.potential / c, ql = l.potential / c, qr = r.potential / c, qt = t.potential / c, qb = b.potential / c;
                const double u = a.east_m_s, v = sign * a.north_m_s;
                const double nextq = q + .5 * lx * (ql - 2 * q + qr - (r.east_m_s - l.east_m_s)) +
                                     .5 * ly * (qt - 2 * q + qb - sign * (b.north_m_s - t.north_m_s));
                result.potential = c * nextq;
                result.east_m_s = u + .5 * lx * (l.east_m_s - 2 * u + r.east_m_s - (qr - ql)) + .5 * ly * (t.east_m_s - 2 * u + b.east_m_s);
                const double nextv = v + .5 * lx * (sign * (l.north_m_s + r.north_m_s) - 2 * v) +
                                     .5 * ly * (sign * (t.north_m_s + b.north_m_s) - 2 * v - (qb - qt));
                result.north_m_s = sign * nextv;
            }
            next[static_cast<std::size_t>(i)] = result;
        }
        return {};
    }
    void face_winds(
        std::span<const FlowCell> a,
        const Domain& d,
        MoistureBoundary boundary,
        std::span<double> east,
        std::span<double> north,
        Work& work
    ) {
        const auto w = d.extents()[0], h = d.extents()[1];
        // Caller precharges F blocks, before these fixed loops.
        static_cast<void>(work);
        for (Index y = 0; y < h; ++y)
            for (Index x = 0; x <= w; ++x) {
                double u = 0;
                if (x > 0 && x < w)
                    u = .5 * (a[static_cast<std::size_t>(x - 1 + y * w)].east_m_s + a[static_cast<std::size_t>(x + y * w)].east_m_s);
                else if (boundary == MoistureBoundary::periodic)
                    u = .5 * (a[static_cast<std::size_t>(w - 1 + y * w)].east_m_s + a[static_cast<std::size_t>(y * w)].east_m_s);
                east[static_cast<std::size_t>(x + y * (w + 1))] = u;
            }
        for (Index y = 0; y <= h; ++y)
            for (Index x = 0; x < w; ++x) {
                double v = 0;
                if (y > 0 && y < h)
                    v = .5 * (a[static_cast<std::size_t>(x + (y - 1) * w)].north_m_s + a[static_cast<std::size_t>(x + y * w)].north_m_s);
                else if (boundary == MoistureBoundary::periodic)
                    v = .5 * (a[static_cast<std::size_t>(x + (h - 1) * w)].north_m_s + a[static_cast<std::size_t>(x)].north_m_s);
                north[static_cast<std::size_t>(x + y * w)] = v;
            }
    }
}
