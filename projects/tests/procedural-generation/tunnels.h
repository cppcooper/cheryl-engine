#pragma once

#include "clearance.h"

namespace ProcGen {
    inline constexpr std::uint32_t tunnel_version = 1;
    inline constexpr std::string_view tunnel_algorithm = "greedy_least_carve_2d";
    inline constexpr CaveBudgets tunnel_hard_limits{65'536, 16 * 1024 * 1024, 100'000'000};

    struct TunnelRequirements {
        Index cells;
        Index storage_bytes;
        Index minimum_work;
        Index pushes_per_search;
    };
    struct TunnelDescriptor {
        Domain domain;
        std::uint32_t version = tunnel_version;
    };
    struct TunnelConnection {
        Index source;
        Index target;
        Index carved;
        Index length; // cardinal edges, NOT carving cost; no length optimality claim
        bool operator==(const TunnelConnection&) const = default;
    };
    struct TunnelMeasurements {
        Index original_floor = 0;
        Index original_components = 0;
        Index floor = 0;
        Index components = 0;
        Index carved = 0;
        Index connections = 0;
        Index route_length = 0; // sum over chosen attachment routes (may overlap)
        bool connected = false; // exactly one nonempty floor component
        bool operator==(const TunnelMeasurements&) const = default;
    };
    struct TunnelUsage {
        Index work = 0;
        Index pushes = 0; // includes duplicates/improvements in all searches
        Index pops = 0;
        Index stale_pops = 0;
        Index peak_queue = 0;
    };

    // Owned replacement AND exact original occupancy; together with the Domain,
    // source() identifies the input without a hash/collision or borrowed lifetime.
    // Binary0=floor/1=solid, index=x+width*y. Copies deep-copy every buffer;
    // const spans expire on destruction/move/assignment. Moved-from values only
    // support destruction/assignment. No persisted recipe or file format.
    class ConnectedCave {
        TunnelDescriptor descriptor_;
        std::vector<std::uint8_t> source_, occupancy_;
        std::vector<TunnelConnection> connections_;
        TunnelMeasurements measured_;
        TunnelUsage usage_;

        ConnectedCave(
            TunnelDescriptor descriptor,
            std::vector<std::uint8_t> source,
            std::vector<std::uint8_t> occupancy,
            std::vector<TunnelConnection> connections,
            TunnelMeasurements measured,
            TunnelUsage usage
        );
        friend Result<ConnectedCave> connect_cave(OccupancyView input, CaveBudgets budget, std::uint32_t version);

    public:
        [[nodiscard]] const TunnelDescriptor& descriptor() const { return descriptor_; }
        [[nodiscard]] std::span<const std::uint8_t> source() const { return source_; }
        [[nodiscard]] std::span<const std::uint8_t> occupancy() const { return occupancy_; }
        [[nodiscard]] std::span<const TunnelConnection> connections() const { return connections_; }
        [[nodiscard]] TunnelMeasurements measurements() const { return measured_; }
        [[nodiscard]] TunnelUsage usage() const { return usage_; }
    };

    // Requested peak payload=4096+139*C, not RSS: source/output/flags 3*C,
    // distance/parent/flood queue 24*C, heap 5*C pairs (80*C), up to C records
    // (32*C). Existing caller-owned inputs/results and allocator overhead excluded.
    // Shape planning allocates no buffers and applies no hard admission caps.
    [[nodiscard]] Result<TunnelRequirements> tunnel_requirements(const Domain& domain);
    // V1: first canonical floor component is root; repeated multi-source Dijkstra
    // chooses a current settled external floor with minimum NEW solid-cell cost.
    // Heap ties=(cost,index), neighbor order X-/X+/Y-/Y+, equal costs keep first
    // parent. Greedy attachments are not a globally minimal tunnel network.
    // Solid perimeter is mandatory: a boundary floor contradicts preserving both
    // all floors and a solid boundary. Otherwise the rectangular interior is
    // carveable; empty succeeds connected=false, one component is unchanged.
    // Budgets affect admission only; no RNG. Every owned buffer, duplicate heap
    // entry/comparison, flood, source scan and reconstruction step is bounded.
    // Refusal publishes nothing; allocation/programming exceptions propagate.
    // Input borrowed only during call, unchanged. Clearance is NOT recomputed:
    // analyze replacement explicitly; old fields describe only the old source.
    [[nodiscard]] Result<ConnectedCave> connect_cave(OccupancyView input, CaveBudgets budget, std::uint32_t version = tunnel_version);
}
