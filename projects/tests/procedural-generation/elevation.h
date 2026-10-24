#pragma once

#include "cave.h"

namespace ProcGen {
    using Height = std::int32_t;
    enum class ElevationReason { dimensions, extent, spacing, height_range, overflow, cells, storage, work, algorithm, version, index };
    struct ElevationFailure {
        FailureKind kind;
        ElevationReason reason;
        bool operator==(const ElevationFailure&) const = default;
    };
    template <class T> using ElevationResult = std::expected<T, ElevationFailure>;
    [[nodiscard]] std::string_view elevation_reason_text(ElevationReason reason);
    inline constexpr Index elevation_max_spacing = 4096;
    inline constexpr CaveBudgets elevation_hard_limits{65'536, 2 * 1024 * 1024, 10'000'000};
    inline constexpr std::uint64_t elevation_lattice_stage = 0x656c65766174696f;
    struct ElevationConfiguration {
        Index spacing = 4;             // integer lattice pitch along both axes, in [1,4096] cells
        Height low = -100, high = 100; // inclusive anchor height interval, never normalized
        bool operator==(const ElevationConfiguration&) const = default;
    };
    enum class ElevationAlgorithm : std::uint32_t { lattice_bilinear = 1 };
    struct ElevationReplay {
        Domain domain;
        StreamKey random;
        ElevationConfiguration requested;
        CaveBudgets budgets;
        ElevationAlgorithm algorithm = ElevationAlgorithm::lattice_bilinear;
        std::uint32_t version = 1;
        bool operator==(const ElevationReplay&) const = default;
    };
    struct ElevationRequirements {
        Index cells;
        std::array<Index, 2> lattice;
        Index lattice_samples, storage_bytes, minimum_work;
    };
    struct ElevationMeasurements {
        Height minimum, maximum;
        Index range; // achieved maximum-minimum, may be less than requested interval
        bool operator==(const ElevationMeasurements&) const = default;
    };
    enum class Surface { land, water };
    // Equality is water. This threshold does not imply ocean connectivity or
    // lake/drainage simulation. Sea level uses the SAME height ticks/datum.
    [[nodiscard]] Surface classify_sea(Height height, Height sea_level);
    struct SeaMeasurements {
        Height sea_level;
        Index land, water, work;
        bool operator==(const SeaMeasurements&) const = default;
    };
    // Independently owned signed integer heights, index=x+width*y (axis0=x
    // fastest, positive y advances rows). One vertical tick above arbitrary0;
    // no meters, physical aspect ratio or realism guarantee. Copies deep-copy.
    // Spans expire on destruction/move/assignment; moved-from only supports
    // destruction/assignment. Borrowing input extent/descriptor ends at return.
    class Elevation {
        ElevationReplay replay_;
        std::vector<Height> heights_;
        ElevationMeasurements measured_;
        Usage usage_;
        Elevation(ElevationReplay replay, std::vector<Height> heights, ElevationMeasurements measured, Usage usage);
        friend ElevationResult<Elevation> replay_elevation(const ElevationReplay& replay);
    public:
        [[nodiscard]] const ElevationReplay& descriptor() const { return replay_; }
        [[nodiscard]] const ElevationConfiguration& effective() const { return replay_.requested; }
        [[nodiscard]] std::span<const Height> heights() const { return heights_; }
        [[nodiscard]] ElevationResult<Height> height(Index index) const;
        [[nodiscard]] ElevationMeasurements measurements() const { return measured_; }
        [[nodiscard]] Usage usage() const { return usage_; }
    };
    // Lattice axis count=ceil((extent-1)/spacing)+1; origin0, pitch spacing.
    // Includes a bracketing anchor outside the finite domain when needed; no
    // periodic wrap/clamping of last segment or cross-domain cropping guarantee.
    // Unit axes have one anchor. Peak4096+4*C+4*L, not RSS. Work minimum7*C+3*L;
    // actual7*C+2*L+raw draws. Every rejection draw consumes work too.
    [[nodiscard]] ElevationResult<ElevationRequirements> elevation_requirements(const Domain& domain, ElevationConfiguration config);
    // V1 child stream: for_stage({derive_stream_state(random),lattice_stage,0}).
    // Sample nonnegative offsets in [0,high-low], x-fast lattice order, including
    // flat ranges/bound1. Interpolate four unsigned offsets with bilinear weights
    // totaling spacing^2; divide ONCE, then add low. This rounds toward -infinity,
    // not toward zero, including negative heights. Exact signed32 full range fits
    // checked u64 intermediates at spacing<=4096. No range normalization/octaves.
    // Sufficient budgets affect admission only. Invalid/unsupported/budget failure
    // publishes nothing; allocation/programming exceptions propagate.
    [[nodiscard]] ElevationResult<Elevation> replay_elevation(const ElevationReplay& replay);
    [[nodiscard]] ElevationResult<Elevation>
    generate_elevation(std::span<const Index> extents, StreamKey random, ElevationConfiguration config, CaveBudgets budget);
    // Independent analysis: no allocation/mutation, one charged unit per cell,
    // exact early work admission; returned summary borrows nothing. Sea level is
    // analysis metadata, not an elevation generation/replay parameter.
    [[nodiscard]] ElevationResult<SeaMeasurements> measure_sea(const Elevation& field, Height sea_level, Index work_budget);
}
