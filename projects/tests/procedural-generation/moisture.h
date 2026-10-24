#pragma once
#include "climate.h"

namespace ProcGen {
    enum class MoistureReason {
        dimensions,
        shape,
        context,
        initial,
        model,
        wind,
        boundary,
        overflow,
        cells,
        storage,
        work,
        ticks,
        substeps,
        numeric,
        version
    };
    struct MoistureFailure {
        FailureKind kind;
        MoistureReason reason;
        Index work = 0;
        bool operator==(const MoistureFailure&) const = default;
    };
    template <class T> using MoistureResult = std::expected<T, MoistureFailure>;
    [[nodiscard]] std::string_view moisture_reason_text(MoistureReason);
    enum class MoistureBoundary { closed = 1, periodic = 2 };
    struct FaceWinds {
        // East[(width+1)*height], north[width*(height+1)], both x-fast.
        // Positive east/north m/s; row/north conversion uses the exact U12 frame.
        // Closed outside faces MUST be0; periodic opposite copies MUST agree.
        // Periodic pairs are processed ONCE; unit-axis self wraps do nothing.
        std::span<const double> east, north;
        MoistureBoundary boundary = MoistureBoundary::closed;
    };
    struct MoistureCell {
        double vapour_mm = 0, cloud_mm = 0, finite_surface_mm = 0;
        double reference_kelvin = 280; // intensive reference-level air proxy, not water mass
        bool operator==(const MoistureCell&) const = default;
    };
    struct MoistureModel {
        // Version1 linear/clamped nominal column-capacity PROXY (not RH/dew point).
        double capacity_at_273_mm = 6, capacity_mm_per_kelvin = .5;
        double minimum_capacity_mm = 0, maximum_capacity_mm = 100;
        double condensation_per_second = 1.0 / 600, cloud_evaporation_per_second = 1.0 / 1800;
        double rain_per_second = 1.0 / 900, surface_evaporation_mm_per_second = .00001;
        std::uint32_t version = 1, numerics_version = 1;
        bool operator==(const MoistureModel&) const = default;
    };
    struct MoistureBudgets {
        Index cells, storage_bytes, work, ticks, substeps_per_tick;
        bool operator==(const MoistureBudgets&) const = default;
    };
    inline constexpr MoistureBudgets moisture_hard_limits{16'384, 16 * 1024 * 1024, 100'000'000, 10, 64};
    inline constexpr Index moisture_fixed_storage = 16'384;
    struct MoistureRequirements {
        Index cells, east_faces, north_faces, interior_faces, start_storage_bytes, advance_storage_bytes, start_work;
    };
    struct MoistureUsage {
        Index work, storage_bytes, substeps_per_tick, substeps, schedule_trials;
    };
    struct MoistureWindInterval {
        MoistureBoundary boundary;
        Index from_tick, ticks;
        bool operator==(const MoistureWindInterval&) const = default;
    };
    struct MoistureTransfer {
        double finite_evaporation_mm = 0, replenished_mm = 0, condensed_mm = 0, cloud_evaporated_mm = 0, rain_mm = 0;
        bool operator==(const MoistureTransfer&) const = default;
    };
    struct WaterLedger {
        double initial_m3 = 0, final_m3 = 0;
        double finite_evaporation_m3 = 0, replenished_m3 = 0, condensed_m3 = 0, cloud_evaporated_m3 = 0, rain_export_m3 = 0;
        double boundary_in_m3 = 0, boundary_out_m3 = 0; // both0 for the two supported boundaries
        double residual_m3 = 0, tolerance_m3 = 0;
    };
    struct MoistureReport {
        Index from_tick, to_tick, seconds_per_tick;
        std::vector<MoistureTransfer> transfers; // current interval, per whole-cell horizontal area
        WaterLedger water;
    };
    struct MoistureAdvance;
    // Owned complete baseline witness + evolving arrays/logical tick/options and
    // last applied constant face-wind interval. No borrowed spans survive. Copies
    // deep-copy; views expire on owner move/assignment/destruction. Moved-from only
    // supports destruction/assignment. A seed cannot replace these arrays/clock.
    class MoistureState {
        ClimateBaseline baseline_;
        std::vector<MoistureCell> cells_;
        MoistureModel model_;
        Index tick_;
        std::vector<double> east_, north_;
        std::optional<MoistureWindInterval> wind_;
        MoistureUsage usage_;
        MoistureState(
            ClimateBaseline,
            std::vector<MoistureCell>,
            MoistureModel,
            Index,
            std::vector<double>,
            std::vector<double>,
            std::optional<MoistureWindInterval>,
            MoistureUsage
        );
        friend MoistureResult<MoistureState>
        start_moisture(const ClimateBaseline&, std::span<const MoistureCell>, MoistureModel, MoistureBudgets);
        friend MoistureResult<MoistureAdvance>
        advance_moisture(const MoistureState&, const ClimateBaseline&, FaceWinds, Index, MoistureBudgets);
    public:
        static constexpr std::string_view algorithm = "face_upwind_moisture";
        [[nodiscard]] const ClimateBaseline& baseline() const { return baseline_; }
        [[nodiscard]] std::span<const MoistureCell> cells() const { return cells_; }
        [[nodiscard]] const MoistureModel& model() const { return model_; }
        [[nodiscard]] Index tick() const { return tick_; }
        [[nodiscard]] std::span<const double> east_faces() const { return east_; }
        [[nodiscard]] std::span<const double> north_faces() const { return north_; }
        [[nodiscard]] std::optional<MoistureWindInterval> wind_interval() const { return wind_; }
        [[nodiscard]] MoistureUsage usage() const { return usage_; }
    };
    struct MoistureAdvance {
        MoistureState state;
        MoistureReport report;
    };
    // Requirements check arithmetic before admission. Uniform dx*dy is the chosen
    // cell area (regional approximation), so mm*area/1000 gives m3. Start peak
    //16384+216*C; advance peak16384+304*C+8*(east_faces+north_faces). Excludes
    // preexisting inputs/held results/caller copies/allocator overhead, not RSS.
    [[nodiscard]] MoistureResult<MoistureRequirements> moisture_requirements(const Domain&);
    // Model1 capacity=clamp(at273+slope*(Tlocal-273.15),minimum,maximum).
    // All phase fractions=min(rate*dt,1). Local T=reference-lapse*(z-reference_m)
    // ONCE; no additional terrain-lift cooling. Optional processes disable at0.
    [[nodiscard]] MoistureResult<double> moisture_capacity(double local_kelvin, const MoistureModel&);
    // Initial nonnegative water<=1e12 mm and reference[250,600]K, with positive
    // finite reservoir permitted only where finite_water_fraction>0. Tick starts
    // at baseline.tick; solar baseline is retained/frozen, not evolved/repainted.
    // Bad arrays/context are checked before output allocation. Work64+8*C.
    [[nodiscard]] MoistureResult<MoistureState>
    start_moisture(const ClimateBaseline&, std::span<const MoistureCell>, MoistureModel, MoistureBudgets);
    // Expected baseline compares complete relevant source/context/derived values,
    // not only IDs or sizes. Admission-budget metadata is deliberately irrelevant.
    // Constant known winds apply throughout requested1..10 whole ticks. Select the
    // smallest n<=min(caller cap,64) whose ACTUAL per-cell outgoing water fraction
    // AND incoming intensive-temperature fraction are<=1 at dt=seconds_per_tick/n.
    // No wind clamping/skipped time. Reference temperature uses advective upwind
    // differences, preserving constants under convergence; water uses conservative
    // shared-face donor retention. First order is diffusive, no exact diagonal shift.
    // Each substep: transport -> finite/replenished evaporation -> condensation OR
    // cloud re-evaporation -> cloud rain export. No latent heat or energy guarantee.
    // Work64+9*C+2*F+n*(4*C+1)+ticks+steps*(7*C+2*D+1), D canonical nonself faces.
    // Runtime charges include unsuccessful schedule trials. Refusal publishes no
    // partial state/report. Only expected failures are caught; allocation/programming
    // exceptions propagate. Same model/numerics/build/platform/order replay only.
    [[nodiscard]] MoistureResult<MoistureAdvance>
    advance_moisture(const MoistureState&, const ClimateBaseline&, FaceWinds, Index ticks, MoistureBudgets);
}
