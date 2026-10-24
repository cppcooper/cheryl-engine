#pragma once
#include "weather.h"

namespace ProcGen {
    enum class RegionalReason {
        shape,
        soil,
        version,
        source,
        region,
        overflow,
        cells,
        fields,
        storage,
        work,
        ticks,
        substeps,
        history,
        numeric,
        weather
    };
    struct RegionalFailure {
        FailureKind kind;
        RegionalReason reason;
        Index work = 0;
        std::optional<WeatherFailure> weather;
    };
    template <class T> using RegionalResult = std::expected<T, RegionalFailure>;
    // Field values count 8-byte array-payload slots (including record padding),
    // separate from fixed metadata. Domain cells per owner, aggregate fields/bytes
    // per request. Preexisting inputs/held results/caller copies/RSS are excluded.
    struct RegionalBudgets {
        Index cells, field_values, storage_bytes, work, ticks, substeps_per_tick, snapshots;
    };
    inline constexpr RegionalBudgets regional_hard_limits{16384, 4'000'000, 32 * 1024 * 1024, 100'000'000, 10, 64, 4};
    struct SoilCell {
        // Whole horizontal-cell area depths. Explicit parameters, no soil inferred
        // from U10 fill, U11 catchments, elevation or a water-colour threshold.
        double water_mm = 0, capacity_mm = 10, infiltration_fraction = 1, evaporation_mm_per_second = 0;
        bool operator==(const SoilCell&) const = default;
    };
    struct RegionalVersion {
        std::uint32_t schema = 1, soil_model = 1, numerics = 1;
        bool operator==(const RegionalVersion&) const = default;
    };
    struct RegionalUsage {
        Index work, storage_bytes, field_values;
    };
    struct RegionalRequirements {
        Index cells, faces, state_storage_bytes, advance_storage_bytes, state_fields, advance_fields, copy_work, minimum_tick_work;
    };
    struct SoilTransfer {
        double precipitation_mm = 0, infiltration_mm = 0, overflow_mm = 0, evaporation_mm = 0;
        bool operator==(const SoilTransfer&) const = default;
    };
    struct RegionalLedger {
        double initial_m3 = 0, final_m3 = 0, replenished_m3 = 0, precipitation_m3 = 0, infiltration_m3 = 0, overflow_export_m3 = 0,
               soil_evaporation_m3 = 0, finite_evaporation_m3 = 0, residual_m3 = 0, tolerance_m3 = 0;
    };
    struct RegionalReport {
        Index from_tick, to_tick, seconds_per_tick;
        std::vector<SoilTransfer> transfers;
        RegionalLedger water;
    };
    struct RegionalAdvance;
    struct RegionalCheckpoint;
    // Composite owner at completed outer ticks. All views borrow this owner only;
    // copies deep-copy. Moved-from supports assignment/destruction only. Callers
    // synchronize adoption. A seed is not a checkpoint. No retained source view.
    class RegionalState {
        WeatherState weather_;
        std::vector<SoilCell> soil_;
        RegionalUsage usage_;
        RegionalState(WeatherState, std::vector<SoilCell>, RegionalUsage);
        friend RegionalResult<RegionalState> start_regional(const WeatherState&, std::span<const SoilCell>, RegionalBudgets);
        friend RegionalResult<RegionalState> resume_regional(const RegionalCheckpoint&, const ClimateBaseline&, RegionalBudgets);
        friend RegionalResult<RegionalAdvance> advance_regional(const RegionalState&, Index, RegionalBudgets);
    public:
        static constexpr std::string_view algorithm = "outer_tick_soil_weather";
        [[nodiscard]] const WeatherState& weather() const { return weather_; }
        [[nodiscard]] std::span<const SoilCell> soil() const { return soil_; }
        [[nodiscard]] RegionalUsage usage() const { return usage_; }
    };
    // Typed in-memory image, not a wire/save format. WeatherState is already an
    // invariant-preserving owned U14 value; raw weather arrays cannot be edited
    // through this image. Soil/version fields are untrusted and revalidated.
    // Includes baseline/frame/calendar/replay, every prognostic/weather reservoir,
    // exact soil parameters, clock, boundary and semantic forcing schedule.
    struct RegionalCheckpoint {
        WeatherState weather;
        std::vector<SoilCell> soil;
        RegionalVersion version{};
    };
    struct RegionalAdvance {
        RegionalState state;
        RegionalReport report;
    };
    class RegionalHistory {
        std::vector<RegionalCheckpoint> snapshots_;
        RegionalUsage usage_;
        RegionalHistory(std::vector<RegionalCheckpoint>, RegionalUsage);
        friend RegionalResult<RegionalHistory>
        retain_regional_history(std::span<const RegionalCheckpoint>, const ClimateBaseline&, RegionalBudgets);
    public:
        [[nodiscard]] std::span<const RegionalCheckpoint> snapshots() const { return snapshots_; }
        [[nodiscard]] RegionalUsage usage() const { return usage_; }
    };
    struct RegionWindow {
        Index x, y, width, height; // half-open in PARENT x-fast cell coordinates
        RegionalFrame frame;       // exact parent physical frame, not a new simulation
        bool operator==(const RegionWindow&) const = default;
    };
    struct RegionalSignals {
        double local_kelvin, column_saturation_proxy, cloud_fog_proxy, dryness_proxy, wind_exposure_m_s, fire_susceptibility_proxy;
        bool operator==(const RegionalSignals&) const = default;
    };
    struct ObservedCell {
        Index cell;
        ClimateCell baseline;
        MoistureCell water;
        FlowCell flow;
        SoilCell soil;
        RegionalSignals signals;
        bool operator==(const ObservedCell&) const = default;
    };
    struct RegionObservation {
        RegionWindow window;
        ClimateDescriptor source;
        WeatherModel weather_model;
        RegionalVersion version;
        Index tick;
        std::vector<ObservedCell> cells; // local x-fast; cell retains parent index
        RegionalUsage usage;
    };
    // C cells,F retained U14 faces,V vertices: owned state/copy payload
    //65536+280*C+8*F bytes, advance131072+688*C+24*F+8*V. Field slots35*C+F
    // and86*C+3*F+V. Copy work64+9*C+F. See records for composed step work.
    // Expected refusal publishes nothing; bad_alloc/programming exceptions escape.
    [[nodiscard]] RegionalResult<RegionalRequirements> regional_requirements(const WeatherState&);
    [[nodiscard]] RegionalResult<RegionalState> start_regional(const WeatherState&, std::span<const SoilCell>, RegionalBudgets);
    // Each tick: U14 weather first, rain goes to infiltration/overflow, then
    // min(soil,rate*seconds) evaporates into vapour for the NEXT weather tick.
    // Regional ledger counts only overflow as export; U14 rain is internal here.
    // Whole request is transactional; <=10 ticks/64 inner steps, no hidden catchup.
    [[nodiscard]] RegionalResult<RegionalAdvance> advance_regional(const RegionalState&, Index ticks, RegionalBudgets);
    [[nodiscard]] RegionalResult<RegionalCheckpoint> capture_regional(const RegionalState&, RegionalBudgets);
    [[nodiscard]] RegionalResult<RegionalState>
    resume_regional(const RegionalCheckpoint&, const ClimateBaseline& expected, RegionalBudgets);
    //1..4 strictly increasing completed-tick images of the exact same source.
    // Refuse excess count/bytes/fields/work, never truncate or silently evict.
    [[nodiscard]] RegionalResult<RegionalHistory>
    retain_regional_history(std::span<const RegionalCheckpoint>, const ClimateBaseline&, RegionalBudgets);
    // Samples authoritative current state, never advances a region. Sleep policy:
    // retain an old observation frozen, then explicitly sample current owner on
    // wake. No independent seeded simulation/halo or distributed seam guarantee.
    // Signals are proxies, not RH/dew point, fire/ignition/flooding or host events.
    [[nodiscard]] RegionalResult<RegionObservation> observe_region(const RegionalState&, RegionWindow, RegionalBudgets);
}
