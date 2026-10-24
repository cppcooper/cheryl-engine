#pragma once

#include "maze.h"

namespace ProcGen {
    inline constexpr std::uint32_t cave_fill_scale = 10000;
    inline constexpr std::uint32_t cave_max_steps = 64;
    enum class CaveConnectivity { retain_components, largest_floor };

    // Fill is an integer probability numerator over 10000, applied only to
    // interior cells. No rounding/clamping; achieved density is measured, not
    // guaranteed. Bit k in birth/survival selects Moore-8 SOLID neighbor count k.
    struct CaveConfiguration {
        std::uint32_t solid_fill = 4500;
        std::uint32_t steps = 4;
        std::uint16_t birth = 0x1e0;    // solid born at 5..8 neighbors
        std::uint16_t survival = 0x1f0; // solid survives at 4..8 neighbors
        CaveConnectivity connectivity = CaveConnectivity::retain_components;
        bool operator==(const CaveConfiguration&) const = default;
    };
    struct CaveBudgets {
        Index cells;
        Index storage_bytes;
        Index work;
        bool operator==(const CaveBudgets&) const = default;
    };
    inline constexpr CaveBudgets cave_hard_limits{1'000'000, 32 * 1024 * 1024, 100'000'000};
    struct CaveRequirements {
        Index cells;
        Index interior_cells;
        Index storage_bytes;
        // Reserve for every possible floor's cardinal flood plus minimum RNG
        // draws; conservative for solid-heavy results. Rejections need more work.
        Index work_reservation;
    };
    enum class CaveAlgorithm : std::uint32_t { cellular_2d = 1 };
    struct CaveReplay {
        Domain domain;
        StreamKey random;
        CaveConfiguration requested;
        CaveBudgets budgets;
        CaveAlgorithm algorithm = CaveAlgorithm::cellular_2d;
        std::uint32_t version = 1;
        bool operator==(const CaveReplay&) const = default;
    };
    struct CaveMeasurements {
        Index initial_interior_solid;
        Index generated_floor; // after smoothing, before component filtering
        Index generated_components;
        Index floor;
        Index solid;
        Index components;
        Index removed_floor;
        Index largest_floor;
        bool connectivity_required;
        bool connected; // exactly one NONEMPTY cardinal-4 floor component
        bool operator==(const CaveMeasurements&) const = default;
    };

    // Owned semantic occupancy: 0=floor, 1=solid, index=x+width*y; x=axis0,
    // y=axis1. Positive y advances rows, with no physical orientation implied.
    // Every perimeter cell is solid, including width/height <=2 (all solid).
    // Copies own their bytes; borrowed views expire on destruction/move/assignment.
    // A moved-from value supports only destruction/assignment.
    class Cave {
        CaveReplay replay_;
        std::vector<std::uint8_t> occupancy_;
        CaveMeasurements measured_;
        Usage usage_;

        Cave(CaveReplay replay, std::vector<std::uint8_t> occupancy, CaveMeasurements measured, Usage usage);
        friend Result<Cave> replay_cave(const CaveReplay& descriptor);

    public:
        [[nodiscard]] const CaveReplay& descriptor() const { return replay_; }
        [[nodiscard]] const Domain& domain() const { return replay_.domain; }
        // Accepted integer options are exact: effective == requested.
        [[nodiscard]] const CaveConfiguration& effective() const { return replay_.requested; }
        [[nodiscard]] std::span<const std::uint8_t> occupancy() const { return occupancy_; }
        [[nodiscard]] CaveMeasurements measurements() const { return measured_; }
        [[nodiscard]] Usage usage() const { return usage_; }
    };

    // Peak payload: two C-byte occupancy buffers (one becomes output), C u64
    // labels and C u64 queue entries, plus 4096 fixed bytes; not exact RSS.
    [[nodiscard]] Result<CaveRequirements> cave_requirements(const Domain& domain, CaveConfiguration config);
    // Cellular v1: synchronous Moore-8 smoothing, fixed solid perimeter, cardinal-4
    // components. Largest ties keep the smallest component root index. Empty
    // success stays empty and reports connected=false even when required=true.
    // Budgets govern admission only; allocation/programming exceptions propagate.
    // Failures publish no partial occupancy or metrics and mutate no prior Cave.
    [[nodiscard]] Result<Cave> replay_cave(const CaveReplay& descriptor);
    [[nodiscard]] Result<Cave>
    generate_cave(std::span<const Index> extents, StreamKey random, CaveConfiguration config, CaveBudgets budgets);
}
