#pragma once

#include "elevation.h"

#include <limits>

namespace ProcGen {
    enum class ClimateReason {
        dimensions,
        shape,
        revision,
        frame,
        projection,
        heights,
        surface,
        calendar,
        model,
        overflow,
        cells,
        storage,
        work,
        version
    };
    struct ClimateFailure {
        FailureKind kind;
        ClimateReason reason;
        Index work = 0; // charged work before refusal, including the rejected cell
        bool operator==(const ClimateFailure&) const = default;
    };
    template <class T> using ClimateResult = std::expected<T, ClimateFailure>;
    [[nodiscard]] std::string_view climate_reason_text(ClimateReason);
    enum class RowNorth { increasing = 1, decreasing = -1 };
    struct RegionalFrame {
        double dx_metres = 0, dy_metres = 0; // missing scales reject
        double tick_metres = 0, datum_metres = std::numeric_limits<double>::quiet_NaN();
        double centre_latitude_degrees = std::numeric_limits<double>::quiet_NaN();
        double centre_longitude_degrees = std::numeric_limits<double>::quiet_NaN();
        RowNorth north = static_cast<RowNorth>(0); // x always increases east
        bool operator==(const RegionalFrame&) const = default;
    };
    struct SolarCalendar {
        // day_seconds[60,1e6], year_days[4,4096], seconds_per_tick[1,1e6].
        // Year has an integer count of solar days; tilt[0,60] degrees.
        Index day_seconds = 0, year_days = 0, seconds_per_tick = 0; // required caller context
        std::int64_t epoch_seconds = 0;                             // epoch phase0: northern spring equinox, Greenwich midnight
        double tilt_degrees = std::numeric_limits<double>::quiet_NaN();
        bool operator==(const SolarCalendar&) const = default;
    };
    struct SunSample {
        double east, north, up;                      // local ENU unit vector TOWARD sun, signed up
        double daily_mean_cosine, daylight_fraction; // horizontal surface, fixed snapshot declination
        bool operator==(const SunSample&) const = default;
    };
    // Constant bounded scalar operation, not an evolving clock. Circular orbit,
    // prograde rotation, no eccentricity/refraction. Global point latitude[-90,90]
    // is supported independently of the restricted regional projection. Horizon
    // |up|<=1e-14 is snapped to0. No daily averaging of terrain shadow/slope.
    // Integer phase reduction avoids epoch+tick*seconds overflow, even at u64 max.
    [[nodiscard]] ClimateResult<SunSample> sample_sun(const SolarCalendar&, Index tick, double latitude_degrees, double longitude_degrees);
    struct ClimateRevision {
        Index terrain, surface; // nonzero caller labels; not hashes or proof of source history
        bool operator==(const ClimateRevision&) const = default;
    };
    struct ClimateSurface {
        double finite_water_fraction = 0, replenished_water_fraction = 0; // nonnegative, sum<=1
        double albedo = 0.25, roughness_metres = 0.1;
        double land_response_seconds = 21'600, water_response_seconds = 432'000;
        bool operator==(const ClimateSurface&) const = default;
    };
    struct ClimateModel {
        double solar_w_m2 = 1361, transmission = 0.65;
        double cooling_floor_kelvin = 260, cooling_w_m2_kelvin = 15;
        double lapse_kelvin_per_metre = 0.0065, reference_metres = 0;
        bool operator==(const ClimateModel&) const = default;
    };
    struct ClimateContext {
        ClimateRevision expected_revision{};
        RegionalFrame frame;
        SolarCalendar calendar;
        ClimateModel model;
        std::uint32_t model_version = 1, numerics_version = 1, surface_version = 1;
        bool operator==(const ClimateContext&) const = default;
    };
    struct ClimateInput {
        Domain domain;
        std::span<const Height> heights;
        std::span<const ClimateSurface> surface;
        ClimateRevision revision;
    };
    inline constexpr CaveBudgets climate_hard_limits{16'384, 8 * 1024 * 1024, 2'000'000};
    inline constexpr Index climate_fixed_storage = 8192;
    struct ClimateRequirements {
        Index cells, storage_bytes, work;
    };
    struct ClimateCell {
        Height height_tick;
        ClimateSurface surface;                   // exact source values, NOT an inferred sea/ocean mask
        double z_metres, east_slope, north_slope; // dz/de,dz/dn, one-sided edges/centred interior
        double latitude_degrees, longitude_degrees;
        SunSample sun;
        double surface_cosine, absorbed_w_m2; // instantaneous, no cast shadows
        double seasonal_reference_kelvin;     // DAILY horizontal absorbed/cooling at reference_metres
        double air_reference_kelvin;          // seasonal reference corrected by explicit lapse
        double instant_target_kelvin;         // slope illumination/cooling + lapse, NOT evolved temperature
        double response_seconds;              // fraction-weighted land/water time constant, retained for later dynamics
        bool operator==(const ClimateCell&) const = default;
    };
    struct ClimateMeasurements {
        double minimum_z_metres, maximum_z_metres;
        double minimum_air_kelvin, maximum_air_kelvin;
        double mean_absorbed_w_m2, mean_daily_cosine;
        double finite_water_cell_equivalents, replenished_water_cell_equivalents;
        Index sunlit_cells;
        bool operator==(const ClimateMeasurements&) const = default;
    };
    struct ClimateDescriptor {
        static constexpr std::string_view algorithm = "regional_solar_baseline";
        Domain domain;
        ClimateRevision revision;
        ClimateContext context;
        Index tick;
        CaveBudgets budgets;
        std::optional<ElevationReplay> elevation; // present only for the owned Elevation overload
        bool operator==(const ClimateDescriptor&) const = default;
    };
    struct ClimateUsage {
        Index work, storage_bytes;
    };
    // Deep-owned exact inputs/versions/context and derived fields. No view survives
    // the synchronous call. Views expire on owner move/assignment/destruction;
    // moved-from supports destruction/assignment only. Caller synchronizes adoption.
    // No weather, moisture reservoir, rainfall, equilibrium or save format implied.
    class ClimateBaseline {
        ClimateDescriptor descriptor_;
        std::vector<ClimateCell> cells_;
        ClimateMeasurements measured_;
        ClimateUsage usage_;
        ClimateBaseline(ClimateDescriptor, std::vector<ClimateCell>, ClimateMeasurements, ClimateUsage);
        friend ClimateResult<ClimateBaseline> derive_climate(ClimateInput, const ClimateContext&, Index, CaveBudgets);
        friend ClimateResult<ClimateBaseline>
        derive_climate(const Elevation&, std::span<const ClimateSurface>, ClimateRevision, const ClimateContext&, Index, CaveBudgets);
    public:
        [[nodiscard]] const ClimateDescriptor& descriptor() const { return descriptor_; }
        [[nodiscard]] std::span<const ClimateCell> cells() const { return cells_; }
        [[nodiscard]] ClimateMeasurements measurements() const { return measured_; }
        [[nodiscard]] ClimateUsage usage() const { return usage_; }
    };
    // Peak8192+sizeof(ClimateCell)*C (184*C on admitted build), no C-sized scratch.
    // Work64+32*C: context64, validation8/cell, output initialization1/cell,
    // derivation/copy/measurement23/cell. Units count bounded blocks, not CPU
    // instructions or RSS. Requirements check arithmetic before hard-cap admission.
    [[nodiscard]] ClimateResult<ClimateRequirements> climate_requirements(const Domain&);
    // Regional equirectangular tangent approximation on sphere R=6371000m.
    // Cell centres symmetric around stated centre; axis0 east, row sign explicit.
    // Width/height<=128, whole footprint<=256km each axis; centre latitude<=60deg,
    // all samples<=65deg, no dateline crossing. dx/dy[1,10000]m; positive tick<=1000m;
    // transformed z/datum within[-12000,12000]m; |slopes|<=10. No global projection.
    // Invalid scalar/array values checked before cell allocation. All failure
    // paths leave inputs/held results unchanged; bad_alloc/programming exceptions
    // propagate. Sufficient budgets affect admission only. Model/numerics v1 uses
    // ordered double/libm operations; replay scoped to same build/platform/order.
    [[nodiscard]] ClimateResult<ClimateBaseline> derive_climate(ClimateInput, const ClimateContext&, Index tick, CaveBudgets);
    [[nodiscard]] ClimateResult<ClimateBaseline>
    derive_climate(const Elevation&, std::span<const ClimateSurface>, ClimateRevision, const ClimateContext&, Index tick, CaveBudgets);
}
