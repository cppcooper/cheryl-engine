#pragma once
#include "moisture.h"

namespace ProcGen {
    enum class WeatherReason {
        dimensions,
        shape,
        context,
        initial,
        model,
        overflow,
        cells,
        storage,
        work,
        ticks,
        substeps,
        speed,
        numeric,
        version,
        forcing
    };
    struct WeatherFailure {
        FailureKind kind;
        WeatherReason reason;
        Index work = 0;
        bool operator==(const WeatherFailure&) const = default;
    };
    template <class T> using WeatherResult = std::expected<T, WeatherFailure>;
    [[nodiscard]] std::string_view weather_reason_text(WeatherReason);
    struct FlowCell {
        double surface_kelvin = 280, potential = 0, east_m_s = 0, north_m_s = 0;
        bool operator==(const FlowCell&) const = default;
    };
    // Up to4 optional semantic curl-potential scales. Vertex sinusoid product,
    // world-metre origin at patch centre, wavelengths=pitch*dx/dy, absolute
    // tick phase. Closed boundaries window POTENTIAL before curl; periodic
    // nonzero scales require both dimensions divisible by pitch. Cosmetic key
    // is retained context only and never read by semantic evolution.
    struct WeatherScale {
        Index wavelength_cells = 4, period_ticks = 8;
        double potential_m2_s2 = 0;
        bool operator==(const WeatherScale&) const = default;
    };
    struct WeatherForcing {
        StreamKey semantic{0, 0x77656174686572, 0}, cosmetic{0, 0x636f736d65746963, 0};
        double origin_east_metres = 0, origin_north_metres = 0;
        Index count = 0;
        std::array<WeatherScale, 4> scales{};
        bool operator==(const WeatherForcing&) const = default;
    };
    struct WeatherModel {
        MoistureModel moisture;
        WeatherForcing forcing;
        MoistureBoundary boundary = MoistureBoundary::closed;
        double wave_m_s = 2, maximum_wind_m_s = 8;          // wave0 disables the entire wave coupling
        double rotation_per_second = 0.0000727220521664304; // signed planetary angular velocity; f=2*Omega*sin(latitude)
        double drag_per_second = 1.0 / 7200, roughness_drag_per_metre_second = .0001;
        double background_east_m_s = 1, background_north_m_s = 0;
        double terrain_acceleration_m_s2 = .00005; // down-slope bias, terrain is not a wall
        double pressure_relaxation_per_second = 1.0 / 1800, thermal_potential_per_kelvin = .15;
        double air_relaxation_per_second = 1.0 / 7200;
        bool thermal_enabled = true, use_surface_response = true;
        double uniform_response_seconds = 3600;
        std::uint32_t version = 1, numerics_version = 1;
        bool operator==(const WeatherModel&) const = default;
    };
    inline constexpr MoistureBudgets weather_hard_limits{16384, 16 * 1024 * 1024, 100'000'000, 10, 64};
    inline constexpr Index weather_fixed_storage = 32768;
    struct WeatherRequirements {
        Index cells, faces, vertices, start_storage_bytes, advance_storage_bytes, start_work;
    };
    struct WeatherTrajectory {
        Index tick;
        double mean_surface_kelvin, mean_reference_kelvin, mean_east_m_s, mean_north_m_s, maximum_speed_m_s, potential_rms, water_m3,
            rain_export_m3;
    };
    struct WeatherUsage {
        Index work, storage_bytes, substeps_per_tick, substeps;
    };
    struct WeatherAdvance;
    // Owned baseline, water, flow and integer clock. Potential is an artificial
    // m2/s2 wave/thermal-driving surrogate, neither hPa nor projection pressure.
    // Views expire on owner move/assignment/destruction; moved-from assign/destroy
    // only. No borrowed source or seed-only checkpoint; copies deep-copy.
    class WeatherState {
        ClimateBaseline baseline_;
        std::vector<MoistureCell> water_;
        std::vector<FlowCell> flow_;
        WeatherModel model_;
        std::vector<double> forcing_east_, forcing_north_;
        Index tick_;
        WeatherUsage usage_;
        WeatherState(
            ClimateBaseline,
            std::vector<MoistureCell>,
            std::vector<FlowCell>,
            WeatherModel,
            Index,
            WeatherUsage,
            std::vector<double>,
            std::vector<double>
        );
        friend WeatherResult<WeatherState>
        start_weather(const ClimateBaseline&, std::span<const MoistureCell>, std::span<const FlowCell>, WeatherModel, MoistureBudgets);
        friend WeatherResult<WeatherAdvance> advance_weather(const WeatherState&, const ClimateBaseline&, Index, MoistureBudgets);
    public:
        static constexpr std::string_view algorithm = "rusanov_regional_weather";
        [[nodiscard]] const ClimateBaseline& baseline() const { return baseline_; }
        [[nodiscard]] std::span<const MoistureCell> water() const { return water_; }
        [[nodiscard]] std::span<const FlowCell> flow() const { return flow_; }
        [[nodiscard]] const WeatherModel& model() const { return model_; }
        [[nodiscard]] Index tick() const { return tick_; }
        [[nodiscard]] WeatherUsage usage() const { return usage_; }
        // Last outer-tick curl acceleration faces, m/s2, direct discrete divergence
        // cancels within roundoff. This does NOT assert divergence-free final wind.
        [[nodiscard]] std::span<const double> forcing_east() const { return forcing_east_; }
        [[nodiscard]] std::span<const double> forcing_north() const { return forcing_north_; }
    };
    struct WeatherReport {
        MoistureReport moisture;
        std::vector<WeatherTrajectory> trajectory;
    };
    struct WeatherAdvance {
        WeatherState state;
        WeatherReport report;
    };
    // Payload model: start32768+248*C+8*F; advance32768+368*C+16*F+8*V
    // bytes, including exact baseline copy, output/scratch/report and latest curl
    // faces. F=(w+1)*h+w*(h+1), V=(w+1)*(h+1). Fixed allowance covers metadata
    // and <=10 trajectory samples. Excludes preexisting owners/allocator/RSS.
    // Work counts bounded blocks, not instructions; full accounting in records.
    [[nodiscard]] WeatherResult<WeatherRequirements> weather_requirements(const Domain&);
    // Bounded scalar exact source kernels, used by the evolving model as well.
    [[nodiscard]] WeatherResult<double> weather_relax(double value, double target, double dt, double rate);
    [[nodiscard]] WeatherResult<std::array<double, 2>> weather_rotate(double east, double north, double coriolis, double dt);
    // Frozen baseline identity, evolving surface/reference air. Solar target is
    // resampled at each absolute OUTER tick and held throughout that tick. One
    // lapse diagnoses local air; surface coupling adds it back to reference level.
    // No latent heat, atmosphere mass/energy, forecast or visual claim.
    [[nodiscard]] WeatherResult<WeatherState>
    start_weather(const ClimateBaseline&, std::span<const MoistureCell>, std::span<const FlowCell>, WeatherModel, MoistureBudgets);
    // Unsplit linear acoustic Rusanov wave + exact split rotation/relaxations and
    // bounded explicit terrain forcing. c*dt*(1/dx+1/dy)<=.5; 2*speed_cap*dt*
    // active_inverse_spacing<=.5 bounds actual outgoing/incoming water fractions,
    // which are checked again. max source rate*dt<=1 is an accuracy admission
    // policy, not a universal stability constant. A conservative terrain/curl
    // acceleration bound also requires a*dt<=.5*speed_cap. Runtime checks reject
    // unsupported wind/potential/temperature without clipping. At most64 substeps/10 ticks.
    // Expected refusal publishes nothing; allocation/programming exceptions escape.
    [[nodiscard]] WeatherResult<WeatherAdvance> advance_weather(const WeatherState&, const ClimateBaseline&, Index ticks, MoistureBudgets);
}
