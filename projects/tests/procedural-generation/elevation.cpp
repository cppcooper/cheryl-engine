#include "elevation.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace ProcGen {
    namespace {
        struct Stop {
            ElevationFailure failure;
        };
        [[noreturn]] void fail(FailureKind kind, ElevationReason reason) {
            throw Stop{{kind, reason}};
        }
        Index add(Index a, Index b) {
            if (a > std::numeric_limits<Index>::max() - b) {
                fail(FailureKind::invalid_input, ElevationReason::overflow);
            }
            return a + b;
        }
        Index multiply(Index a, Index b) {
            if (b != 0 && a > std::numeric_limits<Index>::max() / b) {
                fail(FailureKind::invalid_input, ElevationReason::overflow);
            }
            return a * b;
        }
        void charge(Work& work, Index count = 1) {
            if (!work.consume(count)) {
                fail(FailureKind::budget_refused, ElevationReason::work);
            }
        }
        Index range_of(ElevationConfiguration c) {
            return static_cast<Index>(static_cast<std::int64_t>(c.high) - c.low);
        }
        ElevationRequirements needed_for(const Domain& domain, ElevationConfiguration config) {
            if (domain.dimensions() != 2) {
                fail(FailureKind::invalid_input, ElevationReason::dimensions);
            }
            if (config.spacing == 0 || config.spacing > elevation_max_spacing) {
                fail(FailureKind::invalid_input, ElevationReason::spacing);
            }
            if (config.low > config.high) {
                fail(FailureKind::invalid_input, ElevationReason::height_range);
            }
            ElevationRequirements result{domain.cells(), {}, 0, 0, 0};
            for (std::size_t axis = 0; axis < 2; ++axis) {
                const auto last = domain.extents()[axis] - 1;
                const auto segments = add(last / config.spacing, static_cast<Index>(last % config.spacing != 0));
                static_cast<void>(multiply(segments, config.spacing)); // last anchor coordinate
                result.lattice[axis] = add(segments, 1);
            }
            result.lattice_samples = multiply(result.lattice[0], result.lattice[1]);
            if (result.cells > std::vector<Height>{}.max_size() || result.lattice_samples > std::vector<std::uint32_t>{}.max_size()) {
                fail(FailureKind::invalid_input, ElevationReason::overflow);
            }
            static_cast<void>(multiply(range_of(config), multiply(config.spacing, config.spacing)));
            result.storage_bytes = add(fixed_storage_allowance, add(multiply(4, result.cells), multiply(4, result.lattice_samples)));
            result.minimum_work = add(multiply(7, result.cells), multiply(3, result.lattice_samples));
            return result;
        }
        static_assert(sizeof(Height) == 4 && sizeof(std::uint32_t) == 4);
        static_assert(2 * sizeof(Elevation) + 2 * sizeof(ElevationReplay) + 1024 <= fixed_storage_allowance);
    }
    std::string_view elevation_reason_text(ElevationReason reason) {
        switch (reason) {
            case ElevationReason::dimensions:
                return "elevation requires exactly two axes";
            case ElevationReason::extent:
                return "elevation extents must be positive";
            case ElevationReason::spacing:
                return "lattice spacing must be in [1,4096] cells";
            case ElevationReason::height_range:
                return "inclusive low height must not exceed high height";
            case ElevationReason::overflow:
                return "elevation domain/index/storage/work arithmetic or capacity overflow";
            case ElevationReason::cells:
                return "elevation cell budget refused";
            case ElevationReason::storage:
                return "elevation accounted storage budget refused";
            case ElevationReason::work:
                return "elevation work budget refused or exhausted";
            case ElevationReason::algorithm:
                return "unsupported elevation algorithm";
            case ElevationReason::version:
                return "unsupported elevation replay version";
            case ElevationReason::index:
                return "elevation index outside domain";
        }
        return "unknown elevation failure";
    }
    ElevationResult<ElevationRequirements> elevation_requirements(const Domain& domain, ElevationConfiguration config) {
        try {
            return needed_for(domain, config);
        } catch (const Stop& stop) {
            return std::unexpected(stop.failure);
        }
    }
    Elevation::Elevation(ElevationReplay replay, std::vector<Height> heights, ElevationMeasurements measured, Usage usage)
    : replay_(std::move(replay)), heights_(std::move(heights)), measured_(measured), usage_(usage) {}
    ElevationResult<Height> Elevation::height(Index index) const {
        if (index >= heights_.size()) {
            return std::unexpected(ElevationFailure{FailureKind::invalid_input, ElevationReason::index});
        }
        return heights_[static_cast<std::size_t>(index)];
    }
    ElevationResult<Elevation> replay_elevation(const ElevationReplay& replay) {
        try {
            if (replay.algorithm != ElevationAlgorithm::lattice_bilinear) {
                fail(FailureKind::unsupported, ElevationReason::algorithm);
            }
            if (replay.version != 1) {
                fail(FailureKind::unsupported, ElevationReason::version);
            }
            const auto& domain = replay.domain;
            const auto c = replay.requested;
            const auto needed = needed_for(domain, c);
            const CaveBudgets budget{
                std::min(replay.budgets.cells, elevation_hard_limits.cells),
                std::min(replay.budgets.storage_bytes, elevation_hard_limits.storage_bytes),
                std::min(replay.budgets.work, elevation_hard_limits.work)
            };
            if (needed.cells > budget.cells) {
                fail(FailureKind::budget_refused, ElevationReason::cells);
            }
            if (needed.storage_bytes > budget.storage_bytes) {
                fail(FailureKind::budget_refused, ElevationReason::storage);
            }
            if (needed.minimum_work > budget.work) {
                fail(FailureKind::budget_refused, ElevationReason::work);
            }
            Work work(budget.work);
            Usage usage{};
            charge(work, add(needed.cells, needed.lattice_samples));
            std::vector<Height> heights(static_cast<std::size_t>(needed.cells), 0);
            std::vector<std::uint32_t> lattice(static_cast<std::size_t>(needed.lattice_samples), 0);
            auto stream = IntegerStream::for_stage({derive_stream_state(replay.random), elevation_lattice_stage, 0});
            const auto bound = range_of(c) + 1;
            for (auto& offset : lattice) {
                charge(work);
                const auto before = work.used();
                const auto draw = sample_below(stream, bound, work);
                usage.random_draws += work.used() - before;
                if (!draw) {
                    fail(FailureKind::budget_refused, ElevationReason::work);
                } // positive checked bound
                offset = static_cast<std::uint32_t>(*draw);
            }
            ElevationMeasurements measured{std::numeric_limits<Height>::max(), std::numeric_limits<Height>::min(), 0};
            const auto width = domain.extents()[0], lw = needed.lattice[0], lh = needed.lattice[1], s = c.spacing;
            const auto denominator = s * s;
            for (Index cell = 0; cell < needed.cells; ++cell) {
                charge(work);
                const auto x = cell % width, y = cell / width, ix = x / s, iy = y / s, rx = x % s, ry = y % s;
                const std::array<Index, 2> xs{ix, std::min(ix + 1, lw - 1)}, ys{iy, std::min(iy + 1, lh - 1)};
                const std::array<Index, 2> wx{s - rx, rx}, wy{s - ry, ry};
                Index numerator = 0;
                for (std::size_t row = 0; row < 2; ++row) {
                    for (std::size_t column = 0; column < 2; ++column) {
                        charge(work);
                        numerator +=
                            static_cast<Index>(lattice[static_cast<std::size_t>(ys[row] * lw + xs[column])]) * wx[column] * wy[row];
                    }
                }
                charge(work);
                const auto value =
                    static_cast<Height>(static_cast<std::int64_t>(c.low) + static_cast<std::int64_t>(numerator / denominator));
                heights[static_cast<std::size_t>(cell)] = value;
                measured.minimum = std::min(measured.minimum, value);
                measured.maximum = std::max(measured.maximum, value);
            }
            measured.range = static_cast<Index>(static_cast<std::int64_t>(measured.maximum) - measured.minimum);
            usage.work = work.used();
            return Elevation(replay, std::move(heights), measured, usage);
        } catch (const Stop& stop) {
            return std::unexpected(stop.failure);
        }
    }
    ElevationResult<Elevation>
    generate_elevation(std::span<const Index> extents, StreamKey random, ElevationConfiguration config, CaveBudgets budget) {
        const auto domain = Domain::checked(extents);
        if (!domain) {
            const auto reason = domain.error().reason == Reason::extent
                                    ? ElevationReason::extent
                                    : (domain.error().reason == Reason::overflow ? ElevationReason::overflow : ElevationReason::dimensions);
            return std::unexpected(ElevationFailure{FailureKind::invalid_input, reason});
        }
        return replay_elevation({*domain, random, config, budget});
    }
    Surface classify_sea(Height height, Height sea_level) {
        return height <= sea_level ? Surface::water : Surface::land;
    }
    ElevationResult<SeaMeasurements> measure_sea(const Elevation& field, Height sea_level, Index work_budget) {
        try {
            const auto cells = field.descriptor().domain.cells();
            if (cells > std::min(work_budget, elevation_hard_limits.work)) {
                fail(FailureKind::budget_refused, ElevationReason::work);
            }
            Work work(work_budget);
            SeaMeasurements measured{sea_level, 0, 0, 0};
            for (const auto height : field.heights()) {
                charge(work);
                if (classify_sea(height, sea_level) == Surface::water) {
                    ++measured.water;
                } else {
                    ++measured.land;
                }
            }
            measured.work = work.used();
            return measured;
        } catch (const Stop& stop) {
            return std::unexpected(stop.failure);
        }
    }
}
