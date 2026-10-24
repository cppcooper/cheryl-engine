#pragma once

#include "cave.h"

namespace ProcGen {
    // Caller keeps Domain/bytes alive and unchanged until analysis returns.
    struct OccupancyView {
        const Domain& domain;
        std::span<const std::uint8_t> occupancy;
    };
    inline constexpr std::uint32_t clearance_version = 1;
    inline constexpr std::string_view clearance_algorithm = "cardinal_manhattan_2d";
    inline constexpr CaveBudgets clearance_hard_limits{1'000'000, 32 * 1024 * 1024, 100'000'000};

    struct ClearanceRequirements {
        Index cells;
        Index storage_bytes;
        Index minimum_work;
        Index maximum_work;
    };
    struct ClearanceDescriptor {
        Domain domain;
        Index minimum;
        std::uint32_t version = clearance_version;
    };
    struct ClearanceMeasurements {
        Index floor = 0;
        Index solid = 0;
        Index maximum = 0;
        Index usable = 0;
        Index components = 0; // cardinal-4 regions of the usable mask
        Index largest_region = 0;
        bool operator==(const ClearanceMeasurements&) const = default;
    };

    // Owned analysis in x-fast rows (index=x+width*y). Solid distance0; each
    // floor's distance is the least cardinal steps to ANY solid cell, including
    // the first cell outside the finite rectangle (perimeter floors distance1).
    // This is an integer Manhattan field, not a Euclidean radius or collision
    // guarantee. Input is unchanged; all input borrowing ends with the call.
    // Copies own both buffers; spans expire on owner destruction/move/assignment.
    // A moved-from value only supports destruction/assignment.
    class Clearance {
        ClearanceDescriptor descriptor_;
        std::vector<Index> distances_;
        std::vector<std::uint8_t> usable_;
        ClearanceMeasurements measured_;
        Index work_;

        Clearance(
            ClearanceDescriptor descriptor,
            std::vector<Index> distances,
            std::vector<std::uint8_t> usable,
            ClearanceMeasurements measured,
            Index work
        );
        friend Result<Clearance> analyze_clearance(OccupancyView input, std::int64_t minimum, CaveBudgets budget);

    public:
        [[nodiscard]] const ClearanceDescriptor& descriptor() const { return descriptor_; }
        [[nodiscard]] std::span<const Index> distances() const { return distances_; }
        [[nodiscard]] std::span<const std::uint8_t> usable() const { return usable_; }
        [[nodiscard]] ClearanceMeasurements measurements() const { return measured_; }
        [[nodiscard]] Index work() const { return work_; }
    };

    // Accounted peak=4096+17*C, not RSS: C u64 distances, C byte mask, C u64 queue
    // for both floods; visited flags live temporarily in the unpublished mask.
    // Work=15*C+6*usable cells. Minimum/maximum bracket complete work, not time.
    [[nodiscard]] Result<ClearanceRequirements> clearance_requirements(const Domain& domain, std::int64_t minimum);
    // Minimum must be >=0; all nonnegative int64 values are valid. A usable cell
    // is FLOOR AND distance>=minimum: thresholds0/1 select every floor, large
    // thresholds can select none. All-solid yields max0/usable0/components0.
    // Positive unit-axis domains are valid; empty/malformed occupancy rejects.
    // No owned buffers until structural checks, admission and binary validation.
    // Runtime refusal returns no partial analysis; allocation/programming
    // exceptions propagate. Version1 is analysis metadata, not generator replay.
    [[nodiscard]] Result<Clearance> analyze_clearance(OccupancyView input, std::int64_t minimum, CaveBudgets budget);
}
